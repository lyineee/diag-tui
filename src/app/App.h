#pragma once

#include "doip/DoipClient.h"
#include "uds/UdsClient.h"
#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

enum class NavPage { Dtc, Did, Raw, Session, Settings, COUNT_ };

struct DidValue {
  std::vector<uint8_t> raw;
  std::string display;
  int64_t numeric_value{0};
  bool is_numeric{false};
};

struct AppState {
  bool connected{false};
  bool routing_ok{false};
  bool connecting{false};
  bool discovering{false};
  std::string connected_ip;
  std::string session_name{"Default"};
  std::string status_message;
  std::vector<uint8_t> last_raw_request;
  std::vector<uint8_t> last_raw_response;
  std::string raw_response_text;
  std::vector<uint8_t> last_did_response;
  uint16_t last_did_read{0};
  std::vector<uint8_t> last_dtc_response;
  std::vector<EcuInfo> discovered_ecus;
  std::string config_ip{"127.0.0.1"};
  uint16_t config_source_addr{0x0E00};
  uint16_t config_target_addr{0x0E80};

  // DID values and history
  std::map<uint16_t, DidValue> did_values;
  std::map<uint16_t, std::vector<int>> did_history;

  // Polling
  bool polling_active{false};
  int polling_interval_s{3};
  std::vector<uint16_t> polled_dids;

  std::recursive_mutex mtx;
};

class App {
public:
  App();
  ~App();

  void Init();
  void StartUdpDiscovery();
  void Disconnect();
  void ConnectWithConfig();

  void ReadDtc();
  void ClearDtc();
  void ReadDid(uint16_t did);
  void WriteDid(uint16_t did, const std::vector<uint8_t>& data);
  void SendRaw(const std::vector<uint8_t>& data);
  void ChangeSession(uint8_t session);

  void SetSourceAddress(uint16_t addr);
  void SetTargetAddress(uint16_t addr);

  void StartPolling(const std::vector<uint16_t>& dids, int interval_s);
  void StopPolling();
  bool IsPolling() const;

  std::shared_ptr<DoipClient> GetDoipClient() const;
  std::shared_ptr<UdsClient> GetUdsClient() const;
  AppState& GetState();

private:
  void PollingThread();
  void OnDiagnosticMessage(const DoipMessage& msg);
  void OnDiscovery(const std::vector<EcuInfo>& ecus);
  void OnConnectResult(bool success, const std::string& msg);
  void OnUdsResponse(const DiagnosticResponse& resp);

  std::shared_ptr<DoipClient> doip_;
  std::shared_ptr<UdsClient> uds_;
  AppState state_;
  std::thread polling_thread_;
  std::atomic<bool> polling_stop_{false};
};
