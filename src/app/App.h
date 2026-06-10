#pragma once

#include "doip/DoipClient.h"
#include "uds/UdsClient.h"
#include "uds/UdsTypes.h"
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

enum class NavPage { Dtc, Did, Raw, Session, COUNT_ };

struct AppState {
  bool connected{false};
  bool routing_ok{false};
  std::string connected_ip;
  std::string vin;
  std::string session_name{"Default"};
  std::vector<DtcInfo> dtc_list;
  std::vector<uint8_t> last_raw_request;
  std::vector<uint8_t> last_raw_response;
  std::string raw_response_text;
  std::vector<EcuInfo> discovered_ecus;
  std::mutex mtx;
};

class App {
public:
  App();
  ~App();

  void Init();
  void StartUdpDiscovery();
  void ConnectToEcu(const std::string& ip);
  void Disconnect();

  void ReadDtc();
  void ClearDtc();
  void ReadDid(uint16_t did);
  void WriteDid(uint16_t did, const std::vector<uint8_t>& data);
  void SendRaw(const std::vector<uint8_t>& data);
  void ChangeSession(DiagnosticSession session);

  std::shared_ptr<DoipClient> GetDoipClient() const;
  std::shared_ptr<UdsClient> GetUdsClient() const;
  AppState& GetState();

  void SetOnResponse(std::function<void(const std::string&)> cb);

private:
  void OnUdsResponse(const UdsResponse& resp);
  void OnDiscovery(const std::vector<EcuInfo>& ecus);

  std::shared_ptr<DoipClient> doip_;
  std::shared_ptr<UdsClient> uds_;
  AppState state_;
  std::function<void(const std::string&)> status_cb_;
};
