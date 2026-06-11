/// FuseDiag Test DoIP Server
/// Uses uds-c types + UdsMessage for UDS request/response handling.
/// Listens on UDP+TCP port 13400.
///
/// Build: cmake --build build --target test-doip-server
/// Run:   ./build/test-doip-server

#include "uds/UdsMessage.h"

#include <doiplib/vehicle_id_request.h>
#include <doiplib/vehicle_id_response.h>
#include <doiplib/routing_activation_request.h>
#include <doiplib/routing_activation_response.h>
#include <doiplib/diag_message.h>
#include <doiplib/generic_nack.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <chrono>
#include <csignal>
#include <iostream>
#include <map>
#include <thread>
#include <vector>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>

using namespace DoipLib;

static std::atomic<bool> g_running{true};
static constexpr int kPort = 13400;
static constexpr int kBufferSize = 4096;

// Canned ECU data
static const char* kTestVin = "WDB111111ZZZ99999";
static constexpr uint16_t kEcuLogicalAddr = 0x0E80;
static constexpr uint16_t kTesterAddr = 0x0E00;
static const std::array<uint8_t, 6> kEid = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
static const std::array<uint8_t, 6> kGid = {0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};

// Session tracking
static uint8_t g_current_session = 0x01;  // Default session
static bool g_dtc_cleared = false;       // DTC clear flag

void handle_signal(int) { g_running = false; }

// ── Dynamic DID helpers ──────────────────────────────────────────────

struct DynamicDidState {
  int64_t odometer = 12345;       // starts at 12345 km
  int fuel_level = 85;            // 85%
  int engine_rpm = 3000;          // 3000 RPM base
  int vehicle_speed = 80;         // 80 km/h base
  int phase = 0;                  // oscillation counter
};

static DynamicDidState& GetDidState() {
  static DynamicDidState s;
  return s;
}

static void UpdateDynamicDids() {
  auto& st = GetDidState();
  st.phase++;

  // Odometer: slowly increase (0.01 km per read)
  st.odometer += 1;

  // Fuel level: slowly decrease, bounce when near empty
  st.fuel_level -= (st.fuel_level > 5) ? 1 : -10;

  // Engine RPM: fluctuate between 2000-4000 using triangular wave
  int cycle = st.phase % 20;
  st.engine_rpm = 2000 + (cycle < 10 ? cycle * 200 : (20 - cycle) * 200);

  // Vehicle speed: fluctuate 0-120
  int speed_cycle = st.phase % 30;
  st.vehicle_speed = speed_cycle * 4;
  if (st.vehicle_speed > 120) st.vehicle_speed = 240 - st.vehicle_speed;
}

// ── UDS Response Builder (uses uds-c types) ─────────────────────────

std::vector<uint8_t> BuildUdsResponse(const std::vector<uint8_t>& request) {
  auto req = UdsMessage::ParseRequest(request);
  uint8_t sid = req.mode;

  switch (sid) {
    case 0x10: { // DiagnosticSessionControl
      g_current_session = (uint8_t)req.pid;
      uint8_t data[] = {g_current_session, 0x00, 0x00, 0x00};
      std::cout << "  Session changed to 0x" << std::hex << (int)g_current_session << std::dec << std::endl;
      return UdsMessage::BuildResponse(sid, data, sizeof(data));
    }

    case 0x3E: { // TesterPresent
      return UdsMessage::BuildResponse(sid);
    }

    case 0x19: { // ReadDTCInformation
      if (req.pid == 0x02) {
        if (g_dtc_cleared) {
          std::vector<uint8_t> empty_resp = {0x59, 0x02, 0x00};
          return empty_resp;
        }
        // ReadDTCByStatusMask - return DTCs with dynamic status
        // each DTC: 3 bytes number + 1 byte status
        static int dtc_phase = 0;
        dtc_phase++;

        // Build DTC list with some dynamic status changes
        struct DtcDef { uint8_t b1, b2, b3; };
        static const DtcDef dtc_pool[] = {
          {0x80, 0x03, 0x01},  // P0301 - Cylinder 1 Misfire
          {0x80, 0x03, 0x00},  // P0300 - Random/Multiple Misfire
          {0x80, 0x01, 0x01},  // P0101 - MAF Circuit
          {0x80, 0x42, 0x00},  // P0420 - Catalyst Efficiency
          {0x80, 0x50, 0x00},  // P0500 - VSS Circuit
          {0x80, 0x62, 0x0F},  // P062F - EEPROM Error
          {0x42, 0x01, 0x01},  // C0101 - ABS Lost Comm
          {0x80, 0x68, 0x05},  // P0685 - ECM Relay
          {0x80, 0x60, 0x06},  // P0606 - ECM Processor
          {0x80, 0x56, 0x02},  // P0562 - System Voltage Low
          {0x42, 0x12, 0x01},  // C0121 - ABS Range/Perf
          {0x80, 0x44, 0x02},  // P0442 - EVAP Small Leak
          {0x80, 0x45, 0x05},  // P0455 - EVAP Large Leak
          {0x42, 0x10, 0x01},  // U0101 - Lost Comm TCM
          {0x80, 0x21, 0x01},  // P2101 - Throttle Actuator
          {0x80, 0x50, 0x05},  // P0505 - Idle Control
          {0x80, 0x11, 0x03},  // P0113 - IAT Circuit High
          {0x80, 0x11, 0x07},  // P0117 - ECT Circuit Low
          {0x80, 0x11, 0x08},  // P0118 - ECT Circuit High
        };
        static const int dtc_count = sizeof(dtc_pool) / sizeof(dtc_pool[0]);

        // Status byte builder with dynamic bits
        auto dtc_status = [dtc_phase](int idx, bool always_active) -> uint8_t {
          uint8_t s = 0;
          if (always_active) {
            s |= 0x01;  // testFailed
            s |= 0x08;  // confirmed
            s |= 0x10;  // testNotSinceClear
          } else {
            // Intermittent faults: cycle through states
            int cycle = (dtc_phase + idx * 3) % 12;
            if (cycle < 6) {
              s |= 0x01;  // testFailed
              s |= 0x08;  // confirmed
            }
            if (cycle < 3 || (cycle >= 6 && cycle < 9)) {
              s |= 0x20;  // testFailedThisCycle
            }
            if (cycle % 4 == 1 || cycle % 4 == 2) {
              s |= 0x04;  // pending
            }
            s |= 0x10;  // testNotSinceClear
          }
          return s;
        };

        // Build response: [0x59, 0x02, count, DTC_data...]
        uint8_t count = dtc_count;
        std::vector<uint8_t> resp;
        resp.push_back(0x59);
        resp.push_back(0x02);
        resp.push_back(count);
        for (int i = 0; i < dtc_count; i++) {
          bool always_on = (i < 3);  // first 3 always active
          resp.push_back(dtc_pool[i].b1);
          resp.push_back(dtc_pool[i].b2);
          resp.push_back(dtc_pool[i].b3);
          resp.push_back(dtc_status(i, always_on));
        }
        std::cout << "  ReadDTC: " << dtc_count << " DTCs (phase=" << dtc_phase << ")" << std::endl;
        return resp;
      }
      return UdsMessage::BuildNegativeResponse(sid, NRC_SUB_FUNCTION_NOT_SUPPORTED);
    }

    case 0x14: { // ClearDiagnosticInformation
      g_dtc_cleared = true;
      std::cout << "  ClearDiagnosticInformation - DTCs cleared" << std::endl;
      return UdsMessage::BuildResponse(sid);
    }

    case 0x22: { // ReadDataByIdentifier
      uint16_t did = req.pid;
      std::vector<uint8_t> data;
      data.push_back(sid | 0x40);
      data.push_back((did >> 8) & 0xFF);
      data.push_back(did & 0xFF);

      switch (did) {
        case 0xF190: { // VIN
          data.insert(data.end(), kTestVin, kTestVin + 17);
          break;
        }
        case 0xF1C0: { // Active diagnostic session
          data.push_back(g_current_session);
          break;
        }
        case 0xD001: { // Odometer
          UpdateDynamicDids();
          uint32_t val = (uint32_t)GetDidState().odometer;
          data.push_back((val >> 24) & 0xFF);
          data.push_back((val >> 16) & 0xFF);
          data.push_back((val >> 8) & 0xFF);
          data.push_back(val & 0xFF);
          break;
        }
        case 0xD002: { // Fuel level
          UpdateDynamicDids();
          data.push_back((uint8_t)GetDidState().fuel_level);
          break;
        }
        case 0xD003: { // Engine speed
          UpdateDynamicDids();
          uint16_t rpm = (uint16_t)GetDidState().engine_rpm;
          data.push_back((rpm >> 8) & 0xFF);
          data.push_back(rpm & 0xFF);
          break;
        }
        case 0xD004: { // Vehicle speed
          UpdateDynamicDids();
          data.push_back((uint8_t)GetDidState().vehicle_speed);
          break;
        }
        case 0xF1A0: { // ECU software version
          const char* sw = "FWv2.4.1";
          data.insert(data.end(), sw, sw + strlen(sw));
          break;
        }
        default: {
          return UdsMessage::BuildNegativeResponse(sid, NRC_REQUEST_OUT_OF_RANGE);
        }
      }
      return data;
    }

    case 0x2E: { // WriteDataByIdentifier
      uint16_t did = req.pid;
      std::cout << "  Write DID 0x" << std::hex << did << std::dec
                << " (" << (int)req.payload_length << " bytes)" << std::endl;
      std::vector<uint8_t> data;
      data.push_back(sid | 0x40);
      data.push_back((did >> 8) & 0xFF);
      data.push_back(did & 0xFF);
      return data;
    }

    case 0x11: { // ECUReset
      uint8_t reset_type = (uint8_t)req.pid;
      std::cout << "  ECU Reset type=0x" << std::hex << (int)reset_type << std::dec << std::endl;
      uint8_t data[] = {reset_type};
      return UdsMessage::BuildResponse(sid, data, 1);
    }

    case 0x27: { // SecurityAccess
      uint8_t subfunc = (uint8_t)req.pid;
      if (subfunc == 0x01 || subfunc == 0x03 || subfunc == 0x05 || subfunc == 0x07) {
        // RequestSeed
        uint8_t resp_sub = (uint8_t)(subfunc + 1);
        uint8_t data[] = {resp_sub, 0xAA, 0xBB, 0xCC, 0xDD};
        auto resp = UdsMessage::BuildResponse(sid, data, sizeof(data));
        resp[1] = resp_sub;
        return resp;
      } else if (subfunc == 0x02 || subfunc == 0x04 || subfunc == 0x06 || subfunc == 0x08) {
        // SendKey - always accept
        uint8_t resp_sub = (uint8_t)(subfunc - 1);
        uint8_t data[] = {resp_sub};
        auto resp = UdsMessage::BuildResponse(sid, data, sizeof(data));
        resp[1] = resp_sub;
        return resp;
      }
      return UdsMessage::BuildNegativeResponse(sid, NRC_SUB_FUNCTION_NOT_SUPPORTED);
    }

    default: {
      return UdsMessage::BuildNegativeResponse(sid, NRC_SERVICE_NOT_SUPPORTED);
    }
  }
}

// ── DoIP Message Handler ────────────────────────────────────────────

bool HandleDoipMessage(const std::vector<uint8_t>& raw,
                       std::vector<uint8_t>& response) {
  GenericNackType nack;
  PayloadType ptype;

  if (!Message::TryExtractPayloadType(raw, ptype)) return false;

  switch (ptype) {
    case PayloadType::RoutingActivationRequest: {
      RoutingActivationRequest req;
      if (!req.TryDeserialize(raw, nack)) return false;
      g_current_session = 0x01;
      g_dtc_cleared = false;
      RoutingActivationResponse resp(0x02, req.GetSourceAddress(),
                                      kEcuLogicalAddr,
                                      RoutingActivationResponseType::Successful);
      resp.Serialize(response);
      std::cout << "  RoutingActivation: client=0x"
                << std::hex << req.GetSourceAddress() << std::dec << std::endl;
      return true;
    }

    case PayloadType::DiagMessage: {
      DiagMessage msg;
      if (!msg.TryDeserialize(raw, nack)) return false;
      std::vector<uint8_t> udsReq;
      msg.GetUserData(udsReq);

      std::cout << "  DiagMessage: src=0x" << std::hex << msg.GetSourceAddress()
                << " tgt=0x" << msg.GetTargetAddress()
                << " UDS=";
      for (auto b : udsReq) std::cout << std::hex << (int)b << " ";
      std::cout << std::dec << std::endl;

      auto udsResp = BuildUdsResponse(udsReq);

      DiagMessage diagResp(0x02, kEcuLogicalAddr, msg.GetSourceAddress(), udsResp);
      diagResp.Serialize(response);
      return true;
    }

    default:
      return false;
  }
}

// ── UDP Thread ──────────────────────────────────────────────────────

void UdpThread() {
  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) { perror("UDP socket"); return; }

  int reuse = 1;
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(kPort);
  addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
    perror("UDP bind"); close(sock); return;
  }

  std::cout << "UDP listening on port " << kPort << std::endl;

  uint8_t buf[kBufferSize];
  sockaddr_in client{};
  socklen_t clientLen = sizeof(client);

  while (g_running) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(sock, &fds);
    struct timeval tv{1, 0};
    if (select(sock + 1, &fds, nullptr, nullptr, &tv) <= 0) continue;

    ssize_t len = recvfrom(sock, buf, sizeof(buf), 0,
                           (sockaddr*)&client, &clientLen);
    if (len < 8) continue;

    std::vector<uint8_t> raw(buf, buf + len);
    GenericNackType nack;

    VehicleIdRequest vehReq;
    if (!vehReq.TryDeserialize(raw, nack)) continue;

    std::cout << "UDP VehicleIdRequest received" << std::endl;

    VehicleIdResponse resp(0x02, kTestVin, kEcuLogicalAddr,
                            kEid, kGid, 0x00, 0x01);
    std::vector<uint8_t> respRaw;
    resp.Serialize(respRaw);

    sendto(sock, respRaw.data(), respRaw.size(), 0,
           (sockaddr*)&client, clientLen);
    std::cout << "  -> Sent VehicleAnnouncement" << std::endl;
  }

  close(sock);
}

// ── TCP Handler ─────────────────────────────────────────────────────

void HandleClient(int clientSock) {
  uint8_t buf[kBufferSize];
  size_t offset = 0;

  while (g_running) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(clientSock, &fds);
    struct timeval tv{1, 0};
    int ret = select(clientSock + 1, &fds, nullptr, nullptr, &tv);
    if (ret <= 0) continue;

    ssize_t n = read(clientSock, buf + offset, sizeof(buf) - offset);
    if (n <= 0) {
      std::cout << "  Client disconnected" << std::endl;
      break;
    }

    size_t total = offset + (size_t)n;

    while (total >= 8) {
      uint32_t payloadLen =
          ((uint32_t)buf[4] << 24) | ((uint32_t)buf[5] << 16) |
          ((uint32_t)buf[6] << 8) | (uint32_t)buf[7];
      size_t packetLen = 8 + payloadLen;
      if (total < packetLen) break;

      std::vector<uint8_t> raw(buf, buf + packetLen);
      std::vector<uint8_t> response;

      if (HandleDoipMessage(raw, response)) {
        send(clientSock, response.data(), response.size(), 0);
      }

      if (total > packetLen) {
        std::memmove(buf, buf + packetLen, total - packetLen);
        total -= packetLen;
      } else {
        total = 0;
      }
    }
    offset = total;
  }

  close(clientSock);
}

// ── TCP Thread ──────────────────────────────────────────────────────

void TcpThread() {
  int serverSock = socket(AF_INET, SOCK_STREAM, 0);
  if (serverSock < 0) { perror("TCP socket"); return; }

  int reuse = 1;
  setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(kPort);
  addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(serverSock, (sockaddr*)&addr, sizeof(addr)) < 0) {
    perror("TCP bind"); close(serverSock); return;
  }

  listen(serverSock, 5);
  std::cout << "TCP listening on port " << kPort << std::endl;

  while (g_running) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(serverSock, &fds);
    struct timeval tv{1, 0};
    if (select(serverSock + 1, &fds, nullptr, nullptr, &tv) <= 0) continue;

    sockaddr_in client{};
    socklen_t clientLen = sizeof(client);
    int clientSock = accept(serverSock, (sockaddr*)&client, &clientLen);
    if (clientSock < 0) continue;

    char clientIp[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client.sin_addr, clientIp, sizeof(clientIp));
    std::cout << "TCP client connected: " << clientIp << std::endl;

    std::thread(HandleClient, clientSock).detach();
  }

  close(serverSock);
}

// ── Main ────────────────────────────────────────────────────────────

int main() {
  signal(SIGINT, handle_signal);
  signal(SIGTERM, handle_signal);

  std::cout << "FuseDiag Test DoIP Server (uds-c)" << std::endl;
  std::cout << "Listening on UDP+TCP port " << kPort << std::endl;
  std::cout << "ECU: VIN=" << kTestVin
            << " LogicalAddr=0x" << std::hex << kEcuLogicalAddr << std::dec << std::endl;
  std::cout << "Supported DIDs: F190(VIN) F1C0(session) D001(odo) D002(fuel) D003(rpm) D004(speed) F1A0(sw)" << std::endl;
  std::cout << "Supported DTCs: 19 codes (P0301 P0300 P0101 P0420 P0500 P062F C0101 ...)" << std::endl;
  std::cout << "  First 3 DTCs: always active. Rest: intermittent (dynamic status)" << std::endl;
  std::cout << "  F5=ReadDTC  F6=ClearDTC  Polling=live graph" << std::endl;
  std::cout << "Press Ctrl+C to stop" << std::endl;
  std::cout << std::endl;

  std::thread udp(UdpThread);
  std::thread tcp(TcpThread);

  udp.join();
  tcp.join();

  std::cout << "Server stopped" << std::endl;
  return 0;
}
