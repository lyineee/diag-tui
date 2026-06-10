#include "uds/UdsMessage.h"
#include <cstring>

std::vector<uint8_t> UdsMessage::BuildRequest(const DiagnosticRequest& req) {
  std::vector<uint8_t> bytes;
  bytes.push_back(req.mode);

  uint8_t pid_len = req.pid_length;
  if (pid_len == 0) {
    pid_len = 1;
    if (req.mode > 0x0A && req.mode != 0x3E && (req.pid & 0xFF00) != 0) {
      pid_len = 2;
    }
  }

  if (req.has_pid) {
    for (int i = pid_len - 1; i >= 0; i--) {
      bytes.push_back((req.pid >> (i * 8)) & 0xFF);
    }
  }

  if (req.payload_length > 0) {
    bytes.insert(bytes.end(), req.payload, req.payload + req.payload_length);
  }

  return bytes;
}

std::vector<uint8_t> UdsMessage::BuildResponse(uint8_t sid,
                                                const uint8_t* data,
                                                uint8_t data_len) {
  std::vector<uint8_t> bytes;
  bytes.push_back(sid | 0x40);
  if (data && data_len > 0) {
    bytes.insert(bytes.end(), data, data + data_len);
  }
  return bytes;
}

std::vector<uint8_t> UdsMessage::BuildNegativeResponse(uint8_t sid,
                                                        DiagnosticNegativeResponseCode nrc) {
  return {0x7F, sid, (uint8_t)nrc};
}

DiagnosticRequest UdsMessage::ParseRequest(const std::vector<uint8_t>& raw) {
  DiagnosticRequest req;
  std::memset(&req, 0, sizeof(req));
  if (raw.empty()) return req;

  req.mode = raw[0];

  if (raw.size() >= 2) {
    req.has_pid = true;
    // Heuristic: UDS modes 0x22/0x2E use 2-byte PIDs; most others use 1
    if (raw[0] == 0x22 || raw[0] == 0x2E) {
      req.pid_length = 2;
      if (raw.size() >= 3) {
        req.pid = (raw[1] << 8) | raw[2];
      }
    } else {
      req.pid_length = 1;
      req.pid = raw[1];
    }
  }

  uint8_t pid_bytes = req.has_pid ? req.pid_length : 0;
  if (raw.size() > 1 + pid_bytes) {
    req.payload_length = (uint8_t)(raw.size() - 1 - pid_bytes);
    if (req.payload_length > sizeof(req.payload)) {
      req.payload_length = sizeof(req.payload);
    }
    std::memcpy(req.payload, raw.data() + 1 + pid_bytes, req.payload_length);
  }

  return req;
}

DiagnosticResponse UdsMessage::ParseResponse(const std::vector<uint8_t>& raw) {
  DiagnosticResponse resp;
  std::memset(&resp, 0, sizeof(resp));
  resp.completed = true;
  resp.success = false;

  if (raw.empty()) return resp;

  uint8_t first = raw[0];

  if (first == 0x7F && raw.size() >= 3) {
    resp.success = false;
    resp.mode = raw[1];
    resp.negative_response_code = (DiagnosticNegativeResponseCode)raw[2];
    return resp;
  }

  if (first & 0x40) {
    resp.success = true;
    resp.mode = first & 0xBF;
    resp.has_pid = false;
    if (raw.size() > 1) {
      size_t copy_len = raw.size() - 1;
      if (copy_len > sizeof(resp.payload)) {
        copy_len = sizeof(resp.payload);
      }
      std::memcpy(resp.payload, raw.data() + 1, copy_len);
      resp.payload_length = (uint8_t)copy_len;
    }
    return resp;
  }

  resp.success = false;
  resp.mode = first;
  return resp;
}

std::string UdsMessage::NrcToString(DiagnosticNegativeResponseCode nrc) {
  switch (nrc) {
    case NRC_SUCCESS: return "PositiveResponse";
    case NRC_SERVICE_NOT_SUPPORTED: return "ServiceNotSupported";
    case NRC_SUB_FUNCTION_NOT_SUPPORTED: return "SubFunctionNotSupported";
    case NRC_INCORRECT_LENGTH_OR_FORMAT: return "IncorrectMessageLengthOrInvalidFormat";
    case NRC_CONDITIONS_NOT_CORRECT: return "ConditionsNotCorrect";
    case NRC_REQUEST_OUT_OF_RANGE: return "RequestOutOfRange";
    case NRC_SECURITY_ACCESS_DENIED: return "SecurityAccessDenied";
    case NRC_INVALID_KEY: return "InvalidKey";
    case NRC_TOO_MANY_ATTEMPS: return "ExceededNumberOfAttempts";
    case NRC_TIME_DELAY_NOT_EXPIRED: return "RequiredTimeDelayNotExpired";
    case NRC_RESPONSE_PENDING: return "RequestCorrectlyReceivedResponsePending";
    default: {
      char buf[32];
      snprintf(buf, sizeof(buf), "UnknownNRC_0x%02X", (uint8_t)nrc);
      return buf;
    }
  }
}

DiagnosticRequest UdsMessage::MakeRequest(uint8_t mode, uint16_t pid,
                                           bool has_pid, uint8_t pid_len,
                                           const uint8_t* payload,
                                           uint8_t payload_len) {
  DiagnosticRequest req;
  std::memset(&req, 0, sizeof(req));
  req.arbitration_id = 0;
  req.mode = mode;
  req.has_pid = has_pid;
  req.pid = pid;
  req.pid_length = pid_len;
  if (payload && payload_len > 0) {
    size_t copy_len = payload_len;
    if (copy_len > sizeof(req.payload)) copy_len = sizeof(req.payload);
    std::memcpy(req.payload, payload, copy_len);
    req.payload_length = (uint8_t)copy_len;
  }
  return req;
}
