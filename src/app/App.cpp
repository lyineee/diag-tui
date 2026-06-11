#include "app/App.h"
#include <spdlog/spdlog.h>

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <chrono>

App::App()
    : doip_(std::make_shared<DoipClient>()),
      uds_(std::make_shared<UdsClient>(doip_)) {}

App::~App() { StopPolling(); Disconnect(); }

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
  std::lock_guard<std::recursive_mutex> lock(state_.mtx);
  if (state_.discovering) return;
  state_.discovering = true;
  state_.discovered_ecus.clear();
  state_.status_message = "Discovering ECUs...";
  doip_->AsyncDiscover();
}

void App::ConnectWithConfig() {
  std::lock_guard<std::recursive_mutex> lock(state_.mtx);
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
  StopPolling();
  {
    std::lock_guard<std::recursive_mutex> lock(state_.mtx);
    state_.connecting = false;
    state_.connected = false;
    state_.routing_ok = false;
    state_.connected_ip.clear();
    state_.status_message = "Disconnected";
  }
  doip_->Disconnect();
}

void App::OnConnectResult(bool success, const std::string& msg) {
  std::lock_guard<std::recursive_mutex> lock(state_.mtx);
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
  std::lock_guard<std::recursive_mutex> lock(state_.mtx);
  state_.discovering = false;
  state_.discovered_ecus = ecus;
  state_.status_message = "Discovered " + std::to_string(ecus.size()) + " ECU(s)";
}

void App::OnDiagnosticMessage(const DoipMessage& msg) {
  DiagnosticResponse resp = UdsMessage::ParseResponse(msg.payload);
  OnUdsResponse(resp);
}

void App::OnUdsResponse(const DiagnosticResponse& resp) {
  std::lock_guard<std::recursive_mutex> lock(state_.mtx);

  state_.last_raw_response.assign(resp.payload, resp.payload + resp.payload_length);

  if (resp.success) {
    if (resp.mode == 0x19) {
      state_.status_message = "Read " + std::to_string(resp.payload_length) + " DTC bytes";
      state_.last_dtc_response.assign(resp.payload, resp.payload + resp.payload_length);
    } else if (resp.mode == 0x22) {
      state_.status_message = "DID read successful";
      state_.last_did_response.assign(resp.payload, resp.payload + resp.payload_length);
      if (state_.last_did_read != 0) {
        DidValue val;
        val.raw.assign(resp.payload, resp.payload + resp.payload_length);

        // Try to parse as numeric (up to 8 bytes as big-endian)
        val.is_numeric = (resp.payload_length > 0 && resp.payload_length <= 8);
        if (val.is_numeric) {
          val.numeric_value = 0;
          for (uint8_t i = 0; i < resp.payload_length; i++) {
            val.numeric_value = (val.numeric_value << 8) | resp.payload[i];
          }
        }

        // Build display string
        std::stringstream ss;
        ss << std::hex << std::uppercase << std::setfill('0');
        for (uint8_t i = 0; i < resp.payload_length; i++) {
          ss << std::setw(2) << (int)resp.payload[i] << " ";
          if ((i + 1) % 16 == 0 && i + 1 < resp.payload_length) ss << "\n";
        }
        val.display = ss.str();

        state_.did_values[state_.last_did_read] = val;

        // Update history for numeric values (max 120 points)
        if (val.is_numeric) {
          auto& hist = state_.did_history[state_.last_did_read];
          hist.push_back((int)val.numeric_value);
          if (hist.size() > 120) {
            hist.erase(hist.begin());
          }
        }
      }
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
  std::lock_guard<std::recursive_mutex> lock(state_.mtx);
  state_.config_source_addr = addr;
}

void App::SetTargetAddress(uint16_t addr) {
  std::lock_guard<std::recursive_mutex> lock(state_.mtx);
  state_.config_target_addr = addr;
}

void App::ReadDtc() {
  uds_->ReadDtcByStatusMask(0xFF);
}

void App::ClearDtc() {
  uds_->ClearDiagnosticInformation(0xFFFF);
}

void App::ReadDid(uint16_t did) {
  std::lock_guard<std::recursive_mutex> lock(state_.mtx);
  state_.last_did_read = did;
  state_.last_did_response.clear();
  uds_->ReadDataByIdentifier(did);
}

void App::WriteDid(uint16_t did, const std::vector<uint8_t>& data) {
  uds_->WriteDataByIdentifier(did, data);
}

void App::SendRaw(const std::vector<uint8_t>& data) {
  std::lock_guard<std::recursive_mutex> lock(state_.mtx);
  state_.last_raw_request = data;
  doip_->SendDiagnostic(data);
}

void App::ChangeSession(uint8_t session) {
  uds_->DiagnosticSessionControl(session);
}

// ── Polling ─────────────────────────────────────────────────────────

void App::StartPolling(const std::vector<uint16_t>& dids, int interval_s) {
  StopPolling();
  {
    std::lock_guard<std::recursive_mutex> lock(state_.mtx);
    state_.polled_dids = dids;
    state_.polling_interval_s = interval_s;
    state_.polling_active = true;
  }
  polling_stop_ = false;
  polling_thread_ = std::thread(&App::PollingThread, this);
}

void App::StopPolling() {
  polling_stop_ = true;
  if (polling_thread_.joinable()) polling_thread_.join();
  {
    std::lock_guard<std::recursive_mutex> lock(state_.mtx);
    state_.polling_active = false;
  }
}

bool App::IsPolling() const {
  return polling_stop_ == false && state_.polling_active;
}

void App::PollingThread() {
  while (!polling_stop_) {
    std::this_thread::sleep_for(std::chrono::seconds(1));

    std::vector<uint16_t> dids;
    int interval;
    {
      std::lock_guard<std::recursive_mutex> lock(state_.mtx);
      dids = state_.polled_dids;
      interval = state_.polling_interval_s;
    }

    static int counter = 0;
    counter++;
    if (counter % interval != 0) continue;

    for (auto did : dids) {
      if (polling_stop_) break;
      ReadDid(did);  // sets last_did_read so OnUdsResponse stores in did_values
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
  }
}

std::shared_ptr<DoipClient> App::GetDoipClient() const { return doip_; }
std::shared_ptr<UdsClient> App::GetUdsClient() const { return uds_; }
AppState& App::GetState() { return state_; }
