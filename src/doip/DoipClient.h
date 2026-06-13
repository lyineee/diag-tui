#pragma once

#include "DoipTypes.h"
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class DoipClient {
public:
  using DiagnosticCallback = std::function<void(const DoipMessage&)>;
  using DiscoveryCallback = std::function<void(const std::vector<EcuInfo>&)>;
  using ConnectCallback = std::function<void(bool success, const std::string& msg)>;
  using StatusChangeCallback = std::function<void(bool connected)>;

  DoipClient();
  ~DoipClient();

  void AsyncConnect(const std::string& ip, uint16_t source_addr,
                    uint16_t target_addr, uint16_t port = 13400,
                    ConnectCallback cb = nullptr);
  void AsyncDiscover(int timeout_ms = 3000);
  void Disconnect();

  bool SendDiagnostic(const std::vector<uint8_t>& payload);

  bool IsConnected() const;
  bool IsRoutingActivated() const;
  bool IsDiscovering() const;
  uint16_t GetSourceAddress() const;
  uint16_t GetTargetAddress() const;
  std::string GetConnectedIp() const;

  void SetDiagnosticCallback(DiagnosticCallback cb);
  void SetDiscoveryCallback(DiscoveryCallback cb);
  void SetStatusChangeCallback(StatusChangeCallback cb);

  std::vector<EcuInfo> GetDiscoveredEcuList() const;

private:
  void ConnectThread(const std::string& ip, uint16_t source_addr,
                     uint16_t target_addr, uint16_t port, ConnectCallback cb);
  void DiscoveryThread(int timeout_ms);
  void TcpReceiveThread();
  void HandleUdpResponse(const uint8_t* data, size_t len);
  void HandleTcpResponse(const uint8_t* data, size_t len);

  std::atomic<bool> running_{false};
  std::atomic<bool> connected_{false};
  std::atomic<bool> routing_activated_{false};
  std::atomic<bool> discovering_{false};

  int udp_sock_{-1};
  int tcp_sock_{-1};
  uint16_t source_address_{0x0E00};
  uint16_t target_address_{0x0000};
  std::string connected_ip_;

  std::vector<EcuInfo> discovered_ecus_;
  mutable std::mutex ecu_mutex_;

  DiagnosticCallback diagnostic_cb_;
  DiscoveryCallback discovery_cb_;
  StatusChangeCallback status_cb_;
  mutable std::mutex cb_mutex_;

  std::thread connect_thread_;
  std::thread discovery_thread_;
  std::thread tcp_thread_;
};

class DoipClientError : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};
