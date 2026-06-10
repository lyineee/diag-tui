#include "uds/UdsClient.h"
#include "doip/DoipClient.h"
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstring>

UdsClient::UdsClient(std::shared_ptr<DoipClient> doip) : doip_(std::move(doip)) {
  doip_->SetResponseCallback([this](const DoipMessage& msg) {
    if (msg.payload_type != 0x8001 && msg.payload_type != 0x8002) return;

    UdsResponse resp = ParseResponse(msg.payload);
    if (default_cb_) {
      default_cb_(resp);
    }
  });
}

void UdsClient::DiagnosticSessionControl(DiagnosticSession session,
                                          UdsResponseCallback cb) {
  std::vector<uint8_t> req = {0x10, (uint8_t)session};
  SendRequest(req, [this, session, cb](const UdsResponse& resp) {
    if (resp.success) {
      current_session_ = session;
      spdlog::info("Session changed to 0x{:02X}", (uint8_t)session);
    }
    if (cb) cb(resp);
  });
}

void UdsClient::EcuReset(EcuResetType type, UdsResponseCallback cb) {
  std::vector<uint8_t> req = {0x11, (uint8_t)type};
  SendRequest(req, std::move(cb));
}

void UdsClient::TesterPresent(UdsResponseCallback cb) {
  std::vector<uint8_t> req = {0x3E, 0x80};
  SendRequest(req, std::move(cb));
}

void UdsClient::ReadDtcByStatusMask(uint8_t status_mask,
                                     DtcFormatType format,
                                     UdsResponseCallback cb) {
  std::vector<uint8_t> req = {0x19, 0x02, (uint8_t)format, status_mask};
  SendRequest(req, std::move(cb));
}

void UdsClient::ReadDtcSnapshot(uint32_t dtc_number,
                                 UdsResponseCallback cb) {
  std::vector<uint8_t> req = {0x19, 0x0A};
  req.push_back((dtc_number >> 16) & 0xFF);
  req.push_back((dtc_number >> 8) & 0xFF);
  req.push_back(dtc_number & 0xFF);
  SendRequest(req, std::move(cb));
}

void UdsClient::ClearDiagnosticInformation(uint16_t group,
                                            UdsResponseCallback cb) {
  std::vector<uint8_t> req = {0x14,
                              (uint8_t)(group >> 8),
                              (uint8_t)(group & 0xFF)};
  SendRequest(req, std::move(cb));
}

void UdsClient::ReadDataByIdentifier(uint16_t did,
                                      UdsResponseCallback cb) {
  std::vector<uint8_t> req = {0x22,
                              (uint8_t)(did >> 8),
                              (uint8_t)(did & 0xFF)};
  SendRequest(req, std::move(cb));
}

void UdsClient::WriteDataByIdentifier(uint16_t did,
                                       const std::vector<uint8_t>& data,
                                       UdsResponseCallback cb) {
  std::vector<uint8_t> req = {0x2E,
                              (uint8_t)(did >> 8),
                              (uint8_t)(did & 0xFF)};
  req.insert(req.end(), data.begin(), data.end());
  SendRequest(req, std::move(cb));
}

void UdsClient::SecurityAccess(uint8_t subfunction,
                                const std::vector<uint8_t>& data,
                                UdsResponseCallback cb) {
  std::vector<uint8_t> req = {0x27, subfunction};
  req.insert(req.end(), data.begin(), data.end());
  SendRequest(req, std::move(cb));
}

DiagnosticSession UdsClient::GetCurrentSession() const {
  return current_session_;
}

void UdsClient::SetRequestTimeoutMs(int timeout_ms) {
  timeout_ms_ = timeout_ms;
}

void UdsClient::SetDefaultCallback(UdsResponseCallback cb) {
  default_cb_ = std::move(cb);
}

void UdsClient::SendRequest(const std::vector<uint8_t>& request,
                             UdsResponseCallback cb) {
  spdlog::debug("UDS Request ({} bytes)", request.size());
  if (!doip_->SendDiagnostic(request)) {
    if (cb) {
      UdsResponse resp;
      resp.success = false;
      resp.error_message = "Failed to send DoIP message";
      cb(resp);
    }
  }
}

UdsResponse UdsClient::ParseResponse(const std::vector<uint8_t>& raw) {
  UdsResponse resp;
  if (raw.empty()) {
    resp.success = false;
    resp.error_message = "Empty response";
    return resp;
  }

  uint8_t sid = raw[0];

  if ((sid & 0x40) && sid != 0x7F) {
    resp.success = true;
    resp.sid = sid & 0xBF;
    resp.data.assign(raw.begin() + 1, raw.end());
    return resp;
  }

  if (sid == 0x7F) {
    if (raw.size() < 3) {
      resp.success = false;
      resp.error_message = "Truncated NRC response";
      return resp;
    }
    resp.success = false;
    resp.sid = raw[1];
    resp.nrc = raw[2];
    resp.error_message = NrcToString(resp.nrc);
    return resp;
  }

  resp.success = false;
  resp.error_message = "Unexpected response SID";
  return resp;
}

std::string UdsClient::NrcToString(uint8_t nrc) {
  switch (nrc) {
    case 0x00: return "PositiveResponse";
    case 0x10: return "GeneralReject";
    case 0x11: return "ServiceNotSupported";
    case 0x12: return "SubFunctionNotSupported";
    case 0x13: return "IncorrectMessageLengthOrInvalidFormat";
    case 0x14: return "ResponseTooLong";
    case 0x21: return "BusyRepeatRequest";
    case 0x22: return "ConditionsNotCorrect";
    case 0x24: return "RequestSequenceError";
    case 0x25: return "NoResponseFromSubnetComponent";
    case 0x26: return "FailurePreventsExecutionOfRequestedAction";
    case 0x31: return "RequestOutOfRange";
    case 0x33: return "SecurityAccessDenied";
    case 0x35: return "InvalidKey";
    case 0x36: return "ExceededNumberOfAttempts";
    case 0x37: return "RequiredTimeDelayNotExpired";
    case 0x70: return "UploadDownloadNotAccepted";
    case 0x71: return "TransferDataSuspended";
    case 0x72: return "GeneralProgrammingFailure";
    case 0x73: return "WrongBlockSequenceCounter";
    case 0x78: return "RequestCorrectlyReceivedResponsePending";
    case 0x7E: return "SubFunctionNotSupportedInActiveSession";
    case 0x7F: return "ServiceNotSupportedInActiveSession";
    default: {
      char buf[16];
      snprintf(buf, sizeof(buf), "UnknownNRC_0x%02X", nrc);
      return buf;
    }
  }
}
