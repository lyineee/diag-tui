#pragma once

#include <cstdint>
#include <string>
#include <vector>

#define MAX_UDS_REQUEST_PAYLOAD_LENGTH 127

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DIAGNOSTIC_REQUEST_TYPE_PID,
    DIAGNOSTIC_REQUEST_TYPE_DTC,
    DIAGNOSTIC_REQUEST_TYPE_MIL_STATUS,
    DIAGNOSTIC_REQUEST_TYPE_VIN
} DiagnosticRequestType;

typedef struct {
    uint32_t arbitration_id;
    uint8_t mode;
    bool has_pid;
    uint16_t pid;
    uint8_t pid_length;
    uint8_t payload[MAX_UDS_REQUEST_PAYLOAD_LENGTH];
    uint8_t payload_length;
    bool no_frame_padding;
    DiagnosticRequestType type;
} DiagnosticRequest;

typedef enum {
    NRC_SUCCESS = 0x0,
    NRC_SERVICE_NOT_SUPPORTED = 0x11,
    NRC_SUB_FUNCTION_NOT_SUPPORTED = 0x12,
    NRC_INCORRECT_LENGTH_OR_FORMAT = 0x13,
    NRC_CONDITIONS_NOT_CORRECT = 0x22,
    NRC_REQUEST_OUT_OF_RANGE = 0x31,
    NRC_SECURITY_ACCESS_DENIED = 0x33,
    NRC_INVALID_KEY = 0x35,
    NRC_TOO_MANY_ATTEMPS = 0x36,
    NRC_TIME_DELAY_NOT_EXPIRED = 0x37,
    NRC_RESPONSE_PENDING = 0x78
} DiagnosticNegativeResponseCode;

#ifdef __cplusplus
}
#endif

enum class DiagnosticSession : uint8_t {
  Default = 0x01,
  Extended = 0x03,
  Programming = 0x02,
  SafetySystem = 0x04,
};

enum class EcuResetType : uint8_t {
  HardReset = 0x01,
  KeyOffOnReset = 0x02,
  SoftReset = 0x03,
  FastShutdown = 0x04,
};

enum class DtcFormatType : uint8_t {
  NumberGroup = 0x00,
  StatusGroup = 0x01,
  SeverityGroup = 0x02,
  FullInfo = 0x03,
};

struct DtcInfo {
  uint32_t dtc_number{0};
  uint8_t status{0};
  std::vector<uint8_t> snapshot_data;
};

struct UdsResponse {
  bool success{false};
  uint8_t sid{0};
  uint8_t nrc{0};
  std::vector<uint8_t> data;
  std::string error_message;
};

// Custom response type replacing uds-c's DiagnosticResponse (which has
// fixed 11-byte payload). Uses std::vector for unlimited payload length.
struct DiagResponse {
  bool success{false};
  bool completed{true};
  uint8_t mode{0};
  DiagnosticNegativeResponseCode nrc{NRC_SUCCESS};
  std::vector<uint8_t> payload;
};

struct DidEntry {
  uint16_t did{0};
  std::string name;
  std::string description;
  uint8_t data_size{0};
  bool graphable{false};
  bool expanded{false};
};
