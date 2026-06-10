#include "doip/DoipClient.h"
#include <doiplib/vehicle_id_request.h>
#include <doiplib/vehicle_id_response.h>
#include <doiplib/routing_activation_request.h>
#include <doiplib/routing_activation_response.h>
#include <doiplib/diag_message.h>
#include <doiplib/generic_nack.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstring>
#include <chrono>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>

using namespace DoipLib;

static constexpr int kDoipPort = 13400;
static constexpr int kBufferSize = 4096;

DoipClient::DoipClient() = default;
DoipClient::~DoipClient() { Disconnect(); }

// ── Async Discovery ────────────────────────────────────────────────

void DoipClient::AsyncDiscover(int timeout_ms) {
  if (discovering_.exchange(true)) {
    spdlog::warn("Discovery already in progress");
    return;
  }
  if (discovery_thread_.joinable()) discovery_thread_.join();
  discovery_thread_ = std::thread(&DoipClient::DiscoveryThread, this, timeout_ms);
}

void DoipClient::DiscoveryThread(int timeout_ms) {
  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
    spdlog::error("Failed to create UDP socket");
    discovering_ = false;
    return;
  }

  int broadcast_enable = 1;
  setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable));
  struct timeval tv;
  tv.tv_sec = timeout_ms / 1000;
  tv.tv_usec = (timeout_ms % 1000) * 1000;
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

  sockaddr_in broadcast_addr{};
  broadcast_addr.sin_family = AF_INET;
  broadcast_addr.sin_port = htons(kDoipPort);
  broadcast_addr.sin_addr.s_addr = INADDR_BROADCAST;

  VehicleIdRequest req(0x02);
  std::vector<uint8_t> raw;
  req.Serialize(raw);
  sendto(sock, raw.data(), raw.size(), 0,
         (sockaddr*)&broadcast_addr, sizeof(broadcast_addr));

  spdlog::info("UDP discovery sent, listening...");

  auto start = std::chrono::steady_clock::now();
  uint8_t buf[kBufferSize];
  sockaddr_in sender{};
  socklen_t sender_len = sizeof(sender);

  while (true) {
    auto elapsed = std::chrono::steady_clock::now() - start;
    if (elapsed > std::chrono::seconds(std::max(1, timeout_ms / 1000))) break;

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(sock, &fds);
    struct timeval sel_tv{0, 500000};
    int ret = select(sock + 1, &fds, nullptr, nullptr, &sel_tv);
    if (ret <= 0) continue;

    ssize_t len = recvfrom(sock, buf, sizeof(buf), 0,
                           (sockaddr*)&sender, &sender_len);
    if (len >= 8) {
      HandleUdpResponse(buf, (size_t)len);
    }
  }

  close(sock);
  discovering_ = false;

  std::vector<EcuInfo> ecus;
  {
    std::lock_guard<std::mutex> lock(ecu_mutex_);
    ecus = discovered_ecus_;
  }
  std::lock_guard<std::mutex> cb_lock(cb_mutex_);
  if (discovery_cb_) discovery_cb_(ecus);
}

void DoipClient::HandleUdpResponse(const uint8_t* data, size_t len) {
  std::vector<uint8_t> raw(data, data + len);
  GenericNackType nack;
  VehicleIdResponse resp;
  if (!resp.TryDeserialize(raw, nack)) return;

  EcuInfo ecu;
  ecu.vin = resp.GetVin();
  ecu.logical_address = resp.GetLogicalAddress();
  auto eid = resp.GetEid();
  auto gid = resp.GetGid();
  std::memcpy(ecu.eid, eid.data(), 6);
  std::memcpy(ecu.gid, gid.data(), 6);

  char ip_str[INET_ADDRSTRLEN];
  // sender address comes from recvfrom - but we can't extract it here
  // since sockaddr_in is not passed through. For real usage, store sender info.
  ecu.source_address = "discovered";

  {
    std::lock_guard<std::mutex> lock(ecu_mutex_);
    auto it = std::find_if(discovered_ecus_.begin(), discovered_ecus_.end(),
                           [&](const EcuInfo& e) {
                             return e.logical_address == ecu.logical_address;
                           });
    if (it == discovered_ecus_.end()) {
      discovered_ecus_.push_back(ecu);
      spdlog::info("Discovered ECU: VIN={}, LogicalAddr=0x{:04X}",
                   ecu.vin, ecu.logical_address);
    }
  }
}

// ── Async Connect ──────────────────────────────────────────────────

void DoipClient::AsyncConnect(const std::string& ip, uint16_t source_addr,
                               uint16_t target_addr, uint16_t port,
                               ConnectCallback cb) {
  if (connect_thread_.joinable()) connect_thread_.join();
  connect_thread_ = std::thread(&DoipClient::ConnectThread, this,
                                 ip, source_addr, target_addr, port, std::move(cb));
}

void DoipClient::ConnectThread(const std::string& ip, uint16_t source_addr,
                                uint16_t target_addr, uint16_t port,
                                ConnectCallback cb) {
  // Close previous connection if any
  if (connected_.exchange(false)) {
    running_ = false;
    if (tcp_thread_.joinable()) tcp_thread_.join();
    if (tcp_sock_ >= 0) { close(tcp_sock_); tcp_sock_ = -1; }
  }

  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    if (cb) cb(false, "Failed to create TCP socket");
    return;
  }

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

  // Non-blocking connect with timeout
  fcntl(sock, F_SETFL, O_NONBLOCK);
  connect(sock, (sockaddr*)&addr, sizeof(addr));

  fd_set fdset;
  FD_ZERO(&fdset);
  FD_SET(sock, &fdset);
  struct timeval connect_tv{3, 0};
  if (select(sock + 1, nullptr, &fdset, nullptr, &connect_tv) <= 0) {
    close(sock);
    if (cb) cb(false, "Connection timeout to " + ip);
    return;
  }

  int so_error;
  socklen_t len = sizeof(so_error);
  getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &len);
  if (so_error != 0) {
    close(sock);
    if (cb) cb(false, "Connection failed: " + std::string(strerror(so_error)));
    return;
  }

  // Restore blocking mode
  fcntl(sock, F_SETFL, 0);

  tcp_sock_ = sock;
  connected_ip_ = ip;
  source_address_ = source_addr;
  target_address_ = target_addr;
  connected_ = true;
  running_ = true;

  // Start TCP receive thread
  if (tcp_thread_.joinable()) tcp_thread_.join();
  tcp_thread_ = std::thread(&DoipClient::TcpReceiveThread, this);

  spdlog::info("TCP connected to {}:{}", ip, port);

  // Send routing activation
  RoutingActivationRequest req(0x02, source_addr, 0x00);
  std::vector<uint8_t> raw;
  req.Serialize(raw);
  ssize_t sent = send(tcp_sock_, raw.data(), raw.size(), 0);
  if (sent < 0) {
    spdlog::error("Routing activation send failed");
    if (cb) cb(false, "Failed to send routing activation");
    return;
  }
  spdlog::info("Routing activation sent, source=0x{:04X}", source_addr);

  // Wait for routing activation response (poll for up to 5s)
  auto wait_start = std::chrono::steady_clock::now();
  while (!routing_activated_.load()) {
    if (std::chrono::steady_clock::now() - wait_start > std::chrono::seconds(5)) {
      if (cb) cb(false, "Routing activation timeout");
      return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  spdlog::info("Routing activated for {}", ip);
  if (cb) cb(true, "Connected to " + ip);
}

// ── Disconnect ─────────────────────────────────────────────────────

void DoipClient::Disconnect() {
  if (connect_thread_.joinable()) {
    // Can't easily stop connect thread, but can detach it
    connect_thread_.detach();
  }

  running_ = false;
  connected_ = false;
  routing_activated_ = false;

  // Close sockets to unblock select/recv
  if (tcp_sock_ >= 0) {
    shutdown(tcp_sock_, SHUT_RDWR);
    close(tcp_sock_);
    tcp_sock_ = -1;
  }

  if (tcp_thread_.joinable()) tcp_thread_.join();

  // Discovery thread will clean itself up via timeout
  if (discovery_thread_.joinable()) discovery_thread_.join();

  connected_ip_.clear();
}

// ── TCP Receive ────────────────────────────────────────────────────

void DoipClient::TcpReceiveThread() {
  uint8_t buf[kBufferSize];
  size_t offset = 0;

  while (running_ && connected_) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(tcp_sock_, &fds);
    struct timeval tv{0, 100000};
    int ret = select(tcp_sock_ + 1, &fds, nullptr, nullptr, &tv);
    if (ret <= 0) continue;

    ssize_t n = read(tcp_sock_, buf + offset, sizeof(buf) - offset);
    if (n <= 0) {
      spdlog::warn("TCP connection lost");
      connected_ = false;
      break;
    }

    size_t total = offset + (size_t)n;

    while (total >= 8) {
      uint32_t payload_len =
          ((uint32_t)buf[4] << 24) | ((uint32_t)buf[5] << 16) |
          ((uint32_t)buf[6] << 8) | (uint32_t)buf[7];
      size_t packet_len = 8 + payload_len;
      if (total < packet_len) break;

      std::vector<uint8_t> packet(buf, buf + packet_len);
      HandleTcpResponse(packet.data(), packet_len);

      if (total > packet_len) {
        std::memmove(buf, buf + packet_len, total - packet_len);
        total -= packet_len;
      } else {
        total = 0;
      }
    }
    offset = total;
  }
}

void DoipClient::HandleTcpResponse(const uint8_t* data, size_t len) {
  std::vector<uint8_t> raw(data, data + len);
  GenericNackType nack;

  PayloadType ptype;
  if (!Message::TryExtractPayloadType(raw, ptype)) return;

  if (ptype == PayloadType::RoutingActivationResponse) {
    RoutingActivationResponse resp;
    if (!resp.TryDeserialize(raw, nack)) return;
    target_address_ = resp.GetEntityLogicalAddress();
    routing_activated_ = (resp.GetResponseCode() == RoutingActivationResponseType::Successful);
    if (routing_activated_) {
      spdlog::info("Routing activated: source=0x{:04X}, target=0x{:04X}",
                   resp.GetTesterLogicalAddress(), target_address_);
    }
    return;
  }

  if (ptype != PayloadType::DiagMessage &&
      ptype != PayloadType::DiagMessagePositiveAcknowledgement) {
    return;
  }

  DiagMessage diag;
  if (!diag.TryDeserialize(raw, nack)) return;

  DoipMessage msg;
  msg.payload_type = (uint16_t)ptype;
  msg.source_address = diag.GetSourceAddress();
  msg.target_address = diag.GetTargetAddress();
  diag.GetUserData(msg.payload);

  std::lock_guard<std::mutex> cb_lock(cb_mutex_);
  if (diagnostic_cb_) diagnostic_cb_(msg);
}

// ── Send ───────────────────────────────────────────────────────────

bool DoipClient::SendDiagnostic(const std::vector<uint8_t>& payload) {
  if (!connected_ || tcp_sock_ < 0) return false;
  DiagMessage msg(0x02, source_address_, target_address_, payload);
  std::vector<uint8_t> raw;
  msg.Serialize(raw);
  ssize_t sent = send(tcp_sock_, raw.data(), raw.size(), 0);
  return sent >= 0;
}

// ── Accessors ──────────────────────────────────────────────────────

bool DoipClient::IsConnected() const { return connected_; }
bool DoipClient::IsRoutingActivated() const { return routing_activated_; }
bool DoipClient::IsDiscovering() const { return discovering_; }
uint16_t DoipClient::GetSourceAddress() const { return source_address_; }
uint16_t DoipClient::GetTargetAddress() const { return target_address_; }
std::string DoipClient::GetConnectedIp() const { return connected_ip_; }

void DoipClient::SetDiagnosticCallback(DiagnosticCallback cb) {
  std::lock_guard<std::mutex> lock(cb_mutex_);
  diagnostic_cb_ = std::move(cb);
}

void DoipClient::SetDiscoveryCallback(DiscoveryCallback cb) {
  std::lock_guard<std::mutex> lock(cb_mutex_);
  discovery_cb_ = std::move(cb);
}

std::vector<EcuInfo> DoipClient::GetDiscoveredEcuList() const {
  std::lock_guard<std::mutex> lock(ecu_mutex_);
  return discovered_ecus_;
}
