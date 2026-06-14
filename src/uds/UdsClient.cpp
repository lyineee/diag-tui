#include "uds/UdsClient.h"
#include "doip/DoipClient.h"
#include <spdlog/spdlog.h>

UdsClient::UdsClient(std::shared_ptr<DoipClient> doip) : doip_(std::move(doip)) {
  doip_->SetDiagnosticCallback([this](const DoipMessage& msg) {
    DiagResponse resp = UdsMessage::ParseResponse(msg.payload);
    if (default_cb_) {
      default_cb_(resp);
    }
  });
}

void UdsClient::DiagnosticSessionControl(uint8_t session,
                                          UdsResponseCallback cb) {
  auto req = UdsMessage::MakeRequest(0x10, session, true, 1);
  SendRequest(req, [this, session, cb](const DiagResponse& resp) {
    if (resp.success) {
      current_session_ = session;
      spdlog::info("Session changed to 0x{:02X}", session);
    }
    if (cb) cb(resp);
  });
}

void UdsClient::EcuReset(uint8_t type, UdsResponseCallback cb) {
  auto req = UdsMessage::MakeRequest(0x11, type, true, 1);
  SendRequest(req, std::move(cb));
}

void UdsClient::TesterPresent(UdsResponseCallback cb) {
  auto req = UdsMessage::MakeRequest(0x3E, 0x80, true, 1);
  SendRequest(req, std::move(cb));
}

void UdsClient::ReadDtcByStatusMask(uint8_t status_mask,
                                     UdsResponseCallback cb) {
  uint8_t payload[] = {0x00, status_mask};
  auto req = UdsMessage::MakeRequest(0x19, 0x02, true, 1, payload, 2);
  SendRequest(req, std::move(cb));
}

void UdsClient::ReportNumberOfDTCByStatusMask(uint8_t status_mask,
                                               UdsResponseCallback cb) {
  uint8_t payload[] = {0x00, status_mask};
  auto req = UdsMessage::MakeRequest(0x19, 0x01, true, 1, payload, 2);
  SendRequest(req, std::move(cb));
}

void UdsClient::ReadDtcSnapshotIdentification(uint8_t status_mask,
                                               UdsResponseCallback cb) {
  uint8_t payload[] = {0x00, status_mask};
  auto req = UdsMessage::MakeRequest(0x19, 0x04, true, 1, payload, 2);
  SendRequest(req, std::move(cb));
}

void UdsClient::ReadDtcSnapshotRecordByDTCNumber(
    const std::vector<uint8_t>& dtc_bytes,
    uint8_t snapshot_number,
    UdsResponseCallback cb) {
  uint8_t payload[4] = {dtc_bytes[0], dtc_bytes[1], dtc_bytes[2], snapshot_number};
  auto req = UdsMessage::MakeRequest(0x19, 0x06, false, 0, payload, 4);
  SendRequest(req, std::move(cb));
}

void UdsClient::ClearDiagnosticInformation(uint16_t group,
                                            UdsResponseCallback cb) {
  uint8_t payload[] = {(uint8_t)(group >> 8), (uint8_t)(group & 0xFF)};
  auto req = UdsMessage::MakeRequest(0x14, 0, false, 0, payload, 2);
  SendRequest(req, std::move(cb));
}

void UdsClient::ReadDataByIdentifier(uint16_t did,
                                      UdsResponseCallback cb) {
  auto req = UdsMessage::MakeRequest(0x22, did, true, 2);
  SendRequest(req, std::move(cb));
}

void UdsClient::WriteDataByIdentifier(uint16_t did,
                                       const std::vector<uint8_t>& data,
                                       UdsResponseCallback cb) {
  auto req = UdsMessage::MakeRequest(0x2E, did, true, 2,
                                      data.data(), (uint8_t)data.size());
  SendRequest(req, std::move(cb));
}

void UdsClient::SecurityAccess(uint8_t subfunction,
                                const std::vector<uint8_t>& data,
                                UdsResponseCallback cb) {
  auto req = UdsMessage::MakeRequest(0x27, subfunction, true, 1,
                                      data.data(), (uint8_t)data.size());
  SendRequest(req, std::move(cb));
}

uint8_t UdsClient::GetCurrentSession() const { return current_session_; }

void UdsClient::SetDefaultCallback(UdsResponseCallback cb) {
  default_cb_ = std::move(cb);
}

void UdsClient::SendRequest(const DiagnosticRequest& req,
                             UdsResponseCallback cb) {
  auto bytes = UdsMessage::BuildRequest(req);
  spdlog::debug("UDS Request ({} bytes)", bytes.size());
  if (!doip_->SendDiagnostic(bytes)) {
    if (cb) cb(DiagResponse{});
  }
}
