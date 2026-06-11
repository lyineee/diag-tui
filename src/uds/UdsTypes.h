#pragma once

#include <uds/uds_types.h>
#include <cstdint>
#include <string>
#include <vector>

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
