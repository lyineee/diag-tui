#pragma once

#include <uds/uds_types.h>
#include <string>
#include <vector>

struct UdsMessage {
  static std::vector<uint8_t> BuildRequest(const DiagnosticRequest& req);
  static std::vector<uint8_t> BuildResponse(uint8_t sid,
                                             const uint8_t* data = nullptr,
                                             uint8_t data_len = 0);
  static std::vector<uint8_t> BuildNegativeResponse(uint8_t sid,
                                                     DiagnosticNegativeResponseCode nrc);
  static DiagnosticResponse ParseResponse(const std::vector<uint8_t>& raw);
  static DiagnosticRequest ParseRequest(const std::vector<uint8_t>& raw);
  static std::string NrcToString(DiagnosticNegativeResponseCode nrc);

  static DiagnosticRequest MakeRequest(uint8_t mode, uint16_t pid,
                                        bool has_pid, uint8_t pid_len,
                                        const uint8_t* payload = nullptr,
                                        uint8_t payload_len = 0);
};
