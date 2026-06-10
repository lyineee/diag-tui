#pragma once

#include "DoipTypes.h"
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class DoipClient {
public:
  using ResponseCallback = std::function<void(const DoipMessage&)>;
  using DiscoveryCallback = std::function<void(const std::vector<EcuInfo>&)>;

  DoipClient();
  ~DoipClient();

  bool SendUdpDiscovery(int timeout_ms = 3000);
  bool ConnectTcp(const std::string& ip, uint16_t port = 13400);
  void Disconnect();
  bool IsConnected() const;

  bool SendRoutingActivation(uint16_t source_address = 0x0E00);
  bool SendDiagnostic(const std::vector<uint8_t>& payload);
  bool SendDiagnosticRaw(const uint8_t* data, size_t len);

  void SetResponseCallback(ResponseCallback cb);
  void SetDiscoveryCallback(DiscoveryCallback cb);

  std::vector<EcuInfo> GetDiscoveredEcuList() const;
  EcuInfo GetSelectedEcu() const;
  uint16_t GetSourceAddress() const;
  std::string GetConnectedIp() const;

  bool WaitForRoutingResponse(int timeout_ms = 5000);

private:
  void UdpListenThread();
  void TcpReceiveThread();
  void HandleUdpResponse(const uint8_t* data, size_t len);
  void HandleTcpResponse(const uint8_t* data, size_t len);
  bool ProcessDoipHeader(const uint8_t* data, size_t len, DoipMessage& msg);

  int udp_sock_{-1};
  int tcp_sock_{-1};
  std::atomic<bool> running_{false};
  std::atomic<bool> connected_{false};
  std::atomic<bool> routing_activated_{false};

  std::string connected_ip_;
  uint16_t source_address_{0x0E00};
  uint16_t target_address_{0x0000};

  std::vector<EcuInfo> discovered_ecus_;
  EcuInfo selected_ecu_;
  mutable std::mutex ecu_mutex_;

  ResponseCallback response_cb_;
  DiscoveryCallback discovery_cb_;
  mutable std::mutex cb_mutex_;

  std::thread udp_thread_;
  std::thread tcp_thread_;

  std::mutex routing_mutex_;
  std::condition_variable routing_cv_;
};

class DoipClientError : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};
