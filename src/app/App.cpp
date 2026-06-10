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
  uds_->SetDefaultCallback([this](const DiagnosticResponse& resp) {
    OnUdsResponse(resp);
  });

  doip_->SetDiagnosticCallback([this](const DoipMessage& msg) {
    OnDiagnosticMessage(msg);
  });

  doip_->SetDiscoveryCallback([this](const std::vector<EcuInfo>& ecus) {
    OnDiscovery(ecus);
  });
}

void App::StartUdpDiscovery() {
  std::lock_guard<std::mutex> lock(state_.mtx);
  if (state_.discovering) return;
  state_.discovering = true;
  state_.discovered_ecus.clear();
  state_.status_message = "Discovering ECUs...";
  doip_->AsyncDiscover();
}

void App::ConnectWithConfig() {
  std::lock_guard<std::mutex> lock(state_.mtx);
  if (state_.connecting || state_.connected) return;
  state_.connecting = true;
  state_.status_message = "Connecting to " + state_.config_ip + "...";

  doip_->AsyncConnect(state_.config_ip, state_.config_source_addr,
                       state_.config_target_addr, 13400,
                       [this](bool success, const std::string& msg) {
                         OnConnectResult(success, msg);
                       });
}

void App::Disconnect() {
  {
    std::lock_guard<std::mutex> lock(state_.mtx);
    state_.connecting = false;
    state_.connected = false;
    state_.routing_ok = false;
    state_.connected_ip.clear();
    state_.status_message = "Disconnected";
  }
  doip_->Disconnect();
}

void App::OnConnectResult(bool success, const std::string& msg) {
  std::lock_guard<std::mutex> lock(state_.mtx);
  state_.connecting = false;
  state_.connected = success;
  state_.routing_ok = success;
  state_.status_message = msg;
  if (success) {
    state_.connected_ip = state_.config_ip;
  }
  spdlog::info("Connect result: {} - {}", success, msg);
}

void App::OnDiscovery(const std::vector<EcuInfo>& ecus) {
  std::lock_guard<std::mutex> lock(state_.mtx);
  state_.discovering = false;
  state_.discovered_ecus = ecus;
  state_.status_message = "Discovered " + std::to_string(ecus.size()) + " ECU(s)";
}

void App::OnDiagnosticMessage(const DoipMessage& msg) {
  DiagnosticResponse resp = UdsMessage::ParseResponse(msg.payload);
  OnUdsResponse(resp);
}

void App::OnUdsResponse(const DiagnosticResponse& resp) {
  std::lock_guard<std::mutex> lock(state_.mtx);

  state_.last_raw_response.assign(resp.payload, resp.payload + resp.payload_length);

  if (resp.success) {
    if (resp.mode == 0x19) {
      state_.status_message = "Read " + std::to_string(resp.payload_length) + " DTC bytes";
    } else if (resp.mode == 0x22) {
      state_.status_message = "DID read successful";
      state_.last_did_response.assign(resp.payload, resp.payload + resp.payload_length);
    } else if (resp.mode == 0x2E) {
      state_.status_message = "DID write successful";
    } else if (resp.mode == 0x10) {
      state_.session_name = "Extended";
      state_.status_message = "Session changed";
    } else if (resp.mode == 0x14) {
      state_.status_message = "DTCs cleared";
    } else if (resp.mode == 0x3E) {
      state_.status_message = "TesterPresent OK";
    }
  } else {
    std::stringstream ss;
    ss << "NRC 0x" << std::hex << (int)resp.negative_response_code
       << ": " << UdsMessage::NrcToString(resp.negative_response_code);
    state_.status_message = ss.str();
    state_.raw_response_text = ss.str();
  }

  if (resp.payload_length > 0) {
    std::stringstream hex;
    hex << std::hex;
    for (uint8_t i = 0; i < resp.payload_length; i++) {
      hex << std::setfill('0') << std::setw(2) << (int)resp.payload[i] << " ";
      if ((i + 1) % 16 == 0) hex << "\n";
    }
    state_.raw_response_text = hex.str();
  }
}

void App::SetSourceAddress(uint16_t addr) {
  std::lock_guard<std::mutex> lock(state_.mtx);
  state_.config_source_addr = addr;
}

void App::SetTargetAddress(uint16_t addr) {
  std::lock_guard<std::mutex> lock(state_.mtx);
  state_.config_target_addr = addr;
}

void App::ReadDtc() {
  uds_->ReadDtcByStatusMask(0xFF);
}

void App::ClearDtc() {
  uds_->ClearDiagnosticInformation(0xFFFF);
}

void App::ReadDid(uint16_t did) {
  std::lock_guard<std::mutex> lock(state_.mtx);
  state_.last_did_read = did;
  state_.last_did_response.clear();
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

void App::ChangeSession(uint8_t session) {
  uds_->DiagnosticSessionControl(session);
}

std::shared_ptr<DoipClient> App::GetDoipClient() const { return doip_; }
std::shared_ptr<UdsClient> App::GetUdsClient() const { return uds_; }
AppState& App::GetState() { return state_; }
