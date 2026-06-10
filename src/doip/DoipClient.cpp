#include "doip/DoipClient.h"
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstring>
#include <chrono>
#include <condition_variable>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>

static constexpr int kDoipPort = 13400;
static constexpr size_t kDoipHeaderLen = 8;
static constexpr int kBufferSize = 4096;

DoipClient::DoipClient() = default;
DoipClient::~DoipClient() { Disconnect(); }

bool DoipClient::SendUdpDiscovery(int timeout_ms) {
  udp_sock_ = socket(AF_INET, SOCK_DGRAM, 0);
  if (udp_sock_ < 0) {
    spdlog::error("Failed to create UDP socket");
    return false;
  }

  int broadcast_enable = 1;
  setsockopt(udp_sock_, SOL_SOCKET, SO_BROADCAST, &broadcast_enable,
             sizeof(broadcast_enable));

  struct timeval tv;
  tv.tv_sec = timeout_ms / 1000;
  tv.tv_usec = (timeout_ms % 1000) * 1000;
  setsockopt(udp_sock_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

  sockaddr_in broadcast_addr{};
  broadcast_addr.sin_family = AF_INET;
  broadcast_addr.sin_port = htons(kDoipPort);
  broadcast_addr.sin_addr.s_addr = INADDR_BROADCAST;

  uint8_t req[] = {0x02, 0xFD, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00};
  ssize_t sent = sendto(udp_sock_, req, sizeof(req), 0,
                        (sockaddr*)&broadcast_addr, sizeof(broadcast_addr));
  if (sent < 0) {
    spdlog::error("UDP discovery send failed");
    close(udp_sock_);
    udp_sock_ = -1;
    return false;
  }

  running_ = true;
  udp_thread_ = std::thread(&DoipClient::UdpListenThread, this);

  spdlog::info("UDP discovery sent, waiting {}ms for responses...", timeout_ms);
  return true;
}

void DoipClient::UdpListenThread() {
  uint8_t buf[kBufferSize];
  sockaddr_in sender{};
  socklen_t sender_len = sizeof(sender);
  auto start = std::chrono::steady_clock::now();

  while (running_) {
    auto elapsed = std::chrono::steady_clock::now() - start;
    if (elapsed > std::chrono::seconds(3)) break;

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(udp_sock_, &fds);
    struct timeval tv{0, 500000};

    int ret = select(udp_sock_ + 1, &fds, nullptr, nullptr, &tv);
    if (ret <= 0) continue;

    ssize_t len =
        recvfrom(udp_sock_, buf, sizeof(buf), 0,
                 (sockaddr*)&sender, &sender_len);
    if (len >= (ssize_t)kDoipHeaderLen) {
      HandleUdpResponse(buf, (size_t)len);
    }
  }

  close(udp_sock_);
  udp_sock_ = -1;

  std::vector<EcuInfo> ecus;
  {
    std::lock_guard<std::mutex> lock(ecu_mutex_);
    ecus = discovered_ecus_;
  }

  std::lock_guard<std::mutex> cb_lock(cb_mutex_);
  if (discovery_cb_) {
    discovery_cb_(ecus);
  }
}

void DoipClient::HandleUdpResponse(const uint8_t* data, size_t len) {
  DoipMessage msg;
  if (!ProcessDoipHeader(data, len, msg)) return;

  if (msg.payload_type == (uint16_t)DoipPayloadType::VehicleAnnouncement) {
    EcuInfo ecu;
    size_t offset = 0;

    if (msg.payload.size() >= 17) {
      char vin[18]{};
      std::memcpy(vin, msg.payload.data(), 17);
      ecu.vin = vin;
      offset += 17;

      if (offset + 2 <= msg.payload.size()) {
        ecu.logical_address =
            (msg.payload[offset] << 8) | msg.payload[offset + 1];
        offset += 2;
      }

      if (offset + 6 <= msg.payload.size()) {
        std::memcpy(ecu.eid, msg.payload.data() + offset, 6);
        offset += 6;
      }

      if (offset + 6 <= msg.payload.size()) {
        std::memcpy(ecu.gid, msg.payload.data() + offset, 6);
        offset += 6;
      }

      char src_ip[INET_ADDRSTRLEN];
      // source address not available from the payload directly
      // in a real impl, extract from sender address
      ecu.source_address = "discovered";

      if (offset < msg.payload.size()) {
        ecu.vin_sync_status = msg.payload[offset];
      }

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
  }
}

bool DoipClient::ConnectTcp(const std::string& ip, uint16_t port) {
  if (connected_) Disconnect();

  tcp_sock_ = socket(AF_INET, SOCK_STREAM, 0);
  if (tcp_sock_ < 0) {
    spdlog::error("Failed to create TCP socket");
    return false;
  }

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

  if (connect(tcp_sock_, (sockaddr*)&addr, sizeof(addr)) < 0) {
    spdlog::error("TCP connection to {}:{} failed", ip, port);
    close(tcp_sock_);
    tcp_sock_ = -1;
    return false;
  }

  connected_ip_ = ip;
  connected_ = true;
  running_ = true;

  tcp_thread_ = std::thread(&DoipClient::TcpReceiveThread, this);

  spdlog::info("TCP connected to {}:{}", ip, port);
  return true;
}

void DoipClient::Disconnect() {
  running_ = false;
  connected_ = false;

  if (tcp_thread_.joinable()) tcp_thread_.join();
  if (udp_thread_.joinable()) udp_thread_.join();

  if (tcp_sock_ >= 0) {
    close(tcp_sock_);
    tcp_sock_ = -1;
  }
  if (udp_sock_ >= 0) {
    close(udp_sock_);
    udp_sock_ = -1;
  }

  connected_ip_.clear();
  routing_activated_ = false;
}

bool DoipClient::IsConnected() const {
  return connected_;
}

bool DoipClient::SendRoutingActivation(uint16_t source_address) {
  if (!connected_ || tcp_sock_ < 0) return false;

  source_address_ = source_address;

  uint8_t req[12];
  req[0] = 0x02;
  req[1] = 0xFD;
  req[2] = 0x00;
  req[3] = 0x05;
  req[4] = 0x00;
  req[5] = 0x00;
  req[6] = 0x00;
  req[7] = 0x04;
  req[8] = (source_address >> 8) & 0xFF;
  req[9] = source_address & 0xFF;
  req[10] = 0x00;
  req[11] = 0x00;

  ssize_t sent = send(tcp_sock_, req, sizeof(req), 0);
  if (sent < 0) {
    spdlog::error("Routing activation send failed");
    return false;
  }

  spdlog::info("Routing activation sent, source=0x{:04X}", source_address);
  return true;
}

bool DoipClient::WaitForRoutingResponse(int timeout_ms) {
  std::unique_lock<std::mutex> lock(routing_mutex_);
  return routing_cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                              [this] { return routing_activated_.load(); });
}

bool DoipClient::SendDiagnostic(const std::vector<uint8_t>& payload) {
  return SendDiagnosticRaw(payload.data(), payload.size());
}

bool DoipClient::SendDiagnosticRaw(const uint8_t* data, size_t len) {
  if (!connected_ || tcp_sock_ < 0) return false;

  size_t total_len = kDoipHeaderLen + len + 4;
  std::vector<uint8_t> msg(total_len);

  msg[0] = 0x02;
  msg[1] = 0xFD;
  msg[2] = 0x80;
  msg[3] = 0x01;
  msg[4] = (uint8_t)((len + 4) >> 24);
  msg[5] = (uint8_t)((len + 4) >> 16);
  msg[6] = (uint8_t)((len + 4) >> 8);
  msg[7] = (uint8_t)((len + 4) & 0xFF);
  msg[8] = (source_address_ >> 8) & 0xFF;
  msg[9] = source_address_ & 0xFF;
  msg[10] = (target_address_ >> 8) & 0xFF;
  msg[11] = target_address_ & 0xFF;

  std::memcpy(msg.data() + 12, data, len);

  ssize_t sent = send(tcp_sock_, msg.data(), msg.size(), 0);
  return sent >= 0;
}

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

    ssize_t len = read(tcp_sock_, buf + offset, sizeof(buf) - offset);
    if (len <= 0) {
      spdlog::warn("TCP connection lost");
      connected_ = false;
      break;
    }

    size_t total = offset + (size_t)len;

    while (total >= kDoipHeaderLen) {
      uint32_t payload_len =
          ((uint32_t)buf[4] << 24) | ((uint32_t)buf[5] << 16) |
          ((uint32_t)buf[6] << 8) | (uint32_t)buf[7];
      size_t packet_len = kDoipHeaderLen + payload_len;

      if (total < packet_len) break;

      HandleTcpResponse(buf, packet_len);

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
  DoipMessage msg;
  if (!ProcessDoipHeader(data, len, msg)) return;

  uint16_t ptype = msg.payload_type;

  if (ptype == (uint16_t)DoipPayloadType::RoutingActivationResponse) {
    if (msg.payload.size() >= 5) {
      uint16_t source = (msg.payload[0] << 8) | msg.payload[1];
      uint16_t target = (msg.payload[2] << 8) | msg.payload[3];
      uint8_t code = msg.payload[4];

      target_address_ = target;

      if (code == (uint8_t)RoutingActivationCode::RoutingActivated) {
        routing_activated_ = true;
        spdlog::info("Routing activated: source=0x{:04X}, target=0x{:04X}",
                     source, target);
      } else {
        spdlog::error("Routing activation failed: code=0x{:02X}", code);
      }

      routing_cv_.notify_one();
    }
  }

  if (response_cb_) {
    response_cb_(msg);
  }
}

bool DoipClient::ProcessDoipHeader(const uint8_t* data, size_t len,
                                    DoipMessage& msg) {
  if (len < kDoipHeaderLen) return false;
  if (data[0] != 0x02 || data[1] != 0xFD) return false;

  msg.payload_type = (data[2] << 8) | data[3];
  uint32_t payload_len =
      ((uint32_t)data[4] << 24) | ((uint32_t)data[5] << 16) |
      ((uint32_t)data[6] << 8) | (uint32_t)data[7];

  if (len < kDoipHeaderLen + payload_len) return false;

  msg.payload.assign(data + kDoipHeaderLen,
                     data + kDoipHeaderLen + payload_len);

  uint16_t ptype = msg.payload_type;
  if (ptype == (uint16_t)DoipPayloadType::DiagnosticMessage ||
      ptype == (uint16_t)DoipPayloadType::DiagnosticMessageAck) {
    if (msg.payload.size() >= 4) {
      msg.source_address = (msg.payload[0] << 8) | msg.payload[1];
      msg.target_address = (msg.payload[2] << 8) | msg.payload[3];
      msg.payload.erase(msg.payload.begin(),
                        msg.payload.begin() + 4);
    }
  }

  return true;
}

void DoipClient::SetResponseCallback(ResponseCallback cb) {
  std::lock_guard<std::mutex> lock(cb_mutex_);
  response_cb_ = std::move(cb);
}

void DoipClient::SetDiscoveryCallback(DiscoveryCallback cb) {
  std::lock_guard<std::mutex> lock(cb_mutex_);
  discovery_cb_ = std::move(cb);
}

std::vector<EcuInfo> DoipClient::GetDiscoveredEcuList() const {
  std::lock_guard<std::mutex> lock(ecu_mutex_);
  return discovered_ecus_;
}

EcuInfo DoipClient::GetSelectedEcu() const {
  std::lock_guard<std::mutex> lock(ecu_mutex_);
  return selected_ecu_;
}

uint16_t DoipClient::GetSourceAddress() const { return source_address_; }

std::string DoipClient::GetConnectedIp() const { return connected_ip_; }
