#include "app/App.h"
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/component/event.hpp>
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
  uds_->SetDefaultCallback([this](const DiagResponse& resp) {
    OnUdsResponse(resp);
  });

  doip_->SetDiagnosticCallback([this](const DoipMessage& msg) {
    OnDiagnosticMessage(msg);
  });

  doip_->SetDiscoveryCallback([this](const std::vector<EcuInfo>& ecus) {
    OnDiscovery(ecus);
  });

  doip_->SetStatusChangeCallback([this](bool connected) {
    if (!connected) {
      OnDisconnected();
    }
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

void App::OnDisconnected() {
  StopPolling();
  {
    std::lock_guard<std::recursive_mutex> lock(state_.mtx);
    state_.connected = false;
    state_.routing_ok = false;
    state_.connecting = false;
    state_.connected_ip.clear();
    state_.status_message = "Connection lost";
  }
  if (screen_) {
    screen_->PostEvent(ftxui::Event::Custom);
  }
}

void App::OnDiscovery(const std::vector<EcuInfo>& ecus) {
  std::lock_guard<std::recursive_mutex> lock(state_.mtx);
  state_.discovering = false;
  state_.discovered_ecus = ecus;
  state_.status_message = "Discovered " + std::to_string(ecus.size()) + " ECU(s)";
}

void App::OnDiagnosticMessage(const DoipMessage& msg) {
  DiagResponse resp = UdsMessage::ParseResponse(msg.payload);
  OnUdsResponse(resp);
}

void App::OnUdsResponse(const DiagResponse& resp) {
  std::lock_guard<std::recursive_mutex> lock(state_.mtx);

  state_.last_raw_response = resp.payload;

  bool did_updated = false;
  bool dtc_updated = false;
  bool snapshot_updated = false;

  if (resp.success) {
    if (resp.mode == 0x19) {
      dtc_updated = true;
      state_.last_dtc_response = resp.payload;

      if (resp.payload.size() >= 4 && resp.payload[0] == 0x01) {
        size_t i = 1;
        if (i < resp.payload.size()) i++;
        if (i + 3 <= resp.payload.size()) {
          state_.dtc_count = ((uint32_t)resp.payload[i] << 16) |
                             ((uint32_t)resp.payload[i+1] << 8) |
                             resp.payload[i+2];
        }
        state_.status_message = std::to_string(state_.dtc_count) + " DTC(s) counted";
      } else if (resp.payload.size() >= 1 && resp.payload[0] == 0x04) {
        snapshot_updated = true;
        state_.snapshot_list.clear();
        size_t off = 1;
        if (off < resp.payload.size()) off++;
        while (off + 4 <= resp.payload.size()) {
          DtcInfo dtc;
          dtc.dtc_number = ((uint32_t)resp.payload[off] << 16) |
                           ((uint32_t)resp.payload[off+1] << 8) |
                           resp.payload[off+2];
          dtc.status = resp.payload[off+3];
          dtc.snapshot_count = (off + 5 <= resp.payload.size()) ? resp.payload[off+5] : 0;
          off += 5;
          state_.snapshot_list.push_back(dtc);
        }
        state_.status_message = std::to_string(state_.snapshot_list.size()) + " DTC(s) with snapshots";
      } else if (resp.payload.size() >= 5 && resp.payload[0] == 0x06) {
        snapshot_updated = true;
        state_.selected_snapshot_data.clear();
        if (resp.payload.size() > 5)
          state_.selected_snapshot_data.assign(resp.payload.begin() + 5, resp.payload.end());
        state_.status_message = "Snapshot record (" + std::to_string(state_.selected_snapshot_data.size()) + " bytes)";
      } else {
        state_.dtc_list.clear();
        size_t off = 0;
        if (off < resp.payload.size() && resp.payload[off] == 0x02) off++;
        if (off < resp.payload.size()) off++;

        while (off + 3 <= resp.payload.size()) {
          DtcInfo dtc;
          dtc.dtc_number = ((uint32_t)resp.payload[off] << 16) |
                           ((uint32_t)resp.payload[off+1] << 8) |
                           resp.payload[off+2];
          dtc.status = (off + 3 < resp.payload.size()) ? resp.payload[off+3] : 0;
          off += (off + 4 <= resp.payload.size()) ? 4 : 3;
          state_.dtc_list.push_back(dtc);
        }
        state_.status_message = "Read " + std::to_string(state_.dtc_list.size()) + " DTC(s)";
      }
    } else if (resp.mode == 0x22) {
      did_updated = true;
      state_.status_message = "DID read successful";

      if (resp.payload.size() >= 2) {
        uint16_t did = ((uint16_t)resp.payload[0] << 8) | resp.payload[1];
        uint8_t data_len = (uint8_t)(resp.payload.size() - 2);
        const uint8_t* data_start = resp.payload.data() + 2;

        state_.last_did_read = did;
        state_.last_did_response.assign(data_start, data_start + data_len);

        DidValue val;
        val.raw.assign(data_start, data_start + data_len);

        // Numeric parse on data bytes only (skip DID)
        val.is_numeric = (data_len > 0 && data_len <= 8);
        if (val.is_numeric) {
          val.numeric_value = 0;
          for (uint8_t i = 0; i < data_len; i++) {
            val.numeric_value = (val.numeric_value << 8) | data_start[i];
          }
        }

        // Build display string from raw data
        std::stringstream ss;
        ss << std::hex << std::uppercase << std::setfill('0');
        for (uint8_t i = 0; i < data_len; i++) {
          ss << std::setw(2) << (int)data_start[i] << " ";
          if ((i + 1) % 16 == 0 && i + 1 < data_len) ss << "\n";
        }
        val.display = ss.str();

        state_.did_values[did] = val;

        // If this response matches a manual read, update the manual result too
        if (did == state_.last_manual_did_read) {
          state_.last_manual_did_response.assign(data_start, data_start + data_len);
        }

        // History for numeric values
        if (val.is_numeric) {
          auto& hist = state_.did_history[did];
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
    ss << "NRC 0x" << std::hex << (int)resp.nrc
       << ": " << UdsMessage::NrcToString(resp.nrc);
    state_.status_message = ss.str();
    state_.raw_response_text = ss.str();
  }

  if ((did_updated || dtc_updated || snapshot_updated) && screen_) {
    screen_->PostEvent(ftxui::Event::Custom);
  }

  if (!resp.payload.empty()) {
    std::stringstream hex;
    hex << std::hex;
    for (size_t i = 0; i < resp.payload.size(); i++) {
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
  std::lock_guard<std::recursive_mutex> lock(state_.mtx);
  uds_->ReadDtcByStatusMask(state_.dtc_status_mask);
}

void App::ReadDtcCount() {
  std::lock_guard<std::recursive_mutex> lock(state_.mtx);
  uds_->ReportNumberOfDTCByStatusMask(state_.dtc_status_mask);
}

void App::ReadDtcSnapshots() {
  std::lock_guard<std::recursive_mutex> lock(state_.mtx);
  uds_->ReadDtcSnapshotIdentification(state_.dtc_status_mask);
}

void App::ReadSnapshotRecord(uint32_t dtc_number, uint8_t snapshot_number) {
  std::vector<uint8_t> dtc_bytes(3);
  dtc_bytes[0] = (dtc_number >> 16) & 0xFF;
  dtc_bytes[1] = (dtc_number >> 8) & 0xFF;
  dtc_bytes[2] = dtc_number & 0xFF;
  uds_->ReadDtcSnapshotRecordByDTCNumber(dtc_bytes, snapshot_number);
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

void App::ReadDidManual(uint16_t did) {
  std::lock_guard<std::recursive_mutex> lock(state_.mtx);
  state_.last_manual_did_read = did;
  state_.last_manual_did_response.clear();
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
      if (poll_query_) {
        dids = poll_query_();
      } else {
        dids = state_.polled_dids;
      }
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

void App::SetScreen(ftxui::ScreenInteractive* s) { screen_ = s; }

void App::SetPollQuery(PollDidQuery query) { poll_query_ = std::move(query); }

std::shared_ptr<DoipClient> App::GetDoipClient() const { return doip_; }
std::shared_ptr<UdsClient> App::GetUdsClient() const { return uds_; }
AppState& App::GetState() { return state_; }

void App::RegisterKeyHandler(KeyHandler handler) {
  key_handlers_.push_back(std::move(handler));
}

bool App::HandleGlobalKeys(ftxui::Event event) {
  for (auto& h : key_handlers_)
    if (h(event)) return true;
  return false;
}
