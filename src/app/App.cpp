#include "app/App.h"
#include <spdlog/spdlog.h>

#include <algorithm>
#include <iomanip>
#include <sstream>

App::App()
    : doip_(std::make_shared<DoipClient>()),
      uds_(std::make_shared<UdsClient>(doip_)) {}

App::~App() { Disconnect(); }

void App::Init() {
  uds_->SetDefaultCallback([this](const UdsResponse& resp) {
    OnUdsResponse(resp);
  });

  doip_->SetDiscoveryCallback([this](const std::vector<EcuInfo>& ecus) {
    OnDiscovery(ecus);
  });
}

void App::StartUdpDiscovery() {
  std::lock_guard<std::mutex> lock(state_.mtx);
  state_.discovered_ecus.clear();
  doip_->SendUdpDiscovery();
}

void App::ConnectToEcu(const std::string& ip) {
  std::lock_guard<std::mutex> lock(state_.mtx);
  if (!doip_->ConnectTcp(ip)) {
    if (status_cb_) status_cb_("TCP connection failed");
    return;
  }
  state_.connected_ip = ip;
  state_.connected = true;
  doip_->SendRoutingActivation();

  bool activated = doip_->WaitForRoutingResponse();
  state_.routing_ok = activated;
  if (activated) {
    if (status_cb_) status_cb_("Routing activated, connected to " + ip);
  } else {
    if (status_cb_) status_cb_("Routing activation failed");
  }
}

void App::Disconnect() {
  std::lock_guard<std::mutex> lock(state_.mtx);
  doip_->Disconnect();
  state_.connected = false;
  state_.routing_ok = false;
  state_.connected_ip.clear();
}

void App::ReadDtc() {
  uds_->ReadDtcByStatusMask(0xFF, DtcFormatType::StatusGroup);
}

void App::ClearDtc() {
  uds_->ClearDiagnosticInformation(0xFFFF);
}

void App::ReadDid(uint16_t did) {
  uds_->ReadDataByIdentifier(did);
}

void App::WriteDid(uint16_t did, const std::vector<uint8_t>& data) {
  uds_->WriteDataByIdentifier(did, data);
}

void App::SendRaw(const std::vector<uint8_t>& data) {
  std::lock_guard<std::mutex> lock(state_.mtx);
  state_.last_raw_request = data;
  doip_->SendDiagnostic(data);
}

void App::ChangeSession(DiagnosticSession session) {
  uds_->DiagnosticSessionControl(session);
}

std::shared_ptr<DoipClient> App::GetDoipClient() const { return doip_; }
std::shared_ptr<UdsClient> App::GetUdsClient() const { return uds_; }
AppState& App::GetState() { return state_; }

void App::SetOnResponse(std::function<void(const std::string&)> cb) {
  status_cb_ = std::move(cb);
}

void App::OnUdsResponse(const UdsResponse& resp) {
  std::lock_guard<std::mutex> lock(state_.mtx);

  state_.last_raw_response = resp.data;

  if (resp.success) {
    if (resp.sid == 0x19) {
      // Parse DTC info from response
      state_.dtc_list.clear();
      size_t i = 0;
      while (i < resp.data.size()) {
        DtcInfo dtc;
        if (i + 3 > resp.data.size()) break;
        dtc.dtc_number = ((uint32_t)resp.data[i] << 16) |
                         ((uint32_t)resp.data[i + 1] << 8) | resp.data[i + 2];
        i += 3;
        if (i < resp.data.size()) {
          dtc.status = resp.data[i];
          i++;
        }
        state_.dtc_list.push_back(dtc);
      }

      std::string msg = "Read " + std::to_string(state_.dtc_list.size()) +
                        " DTC(s)";
      if (status_cb_) status_cb_(msg);
    } else if (resp.sid == 0x22 || resp.sid == 0x2E) {
      if (status_cb_) status_cb_("DID operation successful");
    } else if (resp.sid == 0x10) {
      state_.session_name = "Extended";
      if (status_cb_) status_cb_("Session changed successfully");
    } else if (resp.sid == 0x14) {
      state_.dtc_list.clear();
      if (status_cb_) status_cb_("DTCs cleared");
    } else if (resp.sid == 0x3E) {
      // TesterPresent - silent
    } else {
      if (status_cb_) status_cb_("Response SID: 0x" + std::to_string(resp.sid));
    }
  } else {
    // Build raw response text for the Raw page
    std::stringstream ss;
    ss << "NRC 0x" << std::hex << (int)resp.nrc << ": " << resp.error_message;
    state_.raw_response_text = ss.str();
    if (status_cb_) status_cb_(state_.raw_response_text);
  }

  // Build hex dump for raw page
  if (!resp.data.empty()) {
    std::stringstream hex;
    hex << std::hex;
    for (size_t i = 0; i < resp.data.size(); i++) {
      hex << std::setfill('0') << std::setw(2) << (int)resp.data[i] << " ";
      if ((i + 1) % 16 == 0) hex << "\n";
    }
    state_.raw_response_text = hex.str();
  }
}

void App::OnDiscovery(const std::vector<EcuInfo>& ecus) {
  std::lock_guard<std::mutex> lock(state_.mtx);
  state_.discovered_ecus = ecus;
  std::string msg = "Discovered " + std::to_string(ecus.size()) + " ECU(s)";
  if (status_cb_) status_cb_(msg);
}
