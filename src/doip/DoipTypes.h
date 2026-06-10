#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct EcuInfo {
  std::string source_address;
  std::string vin;
  uint16_t logical_address{0};
  uint8_t eid[6]{};
  uint8_t gid[6]{};
  uint8_t vin_sync_status{0};
};

struct DoipMessage {
  uint16_t payload_type{0};
  uint16_t source_address{0};
  uint16_t target_address{0};
  std::vector<uint8_t> payload;
};

enum class DoipPayloadType : uint16_t {
  GenericDoipHeaderNack = 0x0000,
  VehicleIdentificationRequest = 0x0001,
  VehicleIdentificationRequestEid = 0x0002,
  VehicleIdentificationRequestVin = 0x0003,
  VehicleAnnouncement = 0x0004,
  RoutingActivationRequest = 0x0005,
  RoutingActivationResponse = 0x0006,
  AliveCheckRequest = 0x0007,
  AliveCheckResponse = 0x0008,
  DiagnosticMessage = 0x8001,
  DiagnosticMessageAck = 0x8002,
};

enum class RoutingActivationCode : uint8_t {
  UnknownSourceAddress = 0x00,
  RoutingActivated = 0x10,
  WrongActivationLine = 0x11,
  ConfirmationRequired = 0x12,
  AlreadyActivated = 0x13,
};
