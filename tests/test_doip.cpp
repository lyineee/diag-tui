#include <gtest/gtest.h>
#include "doip/DoipTypes.h"
#include <cstring>
#include <vector>

TEST(DoipTypesTest, PayloadTypeValues) {
  EXPECT_EQ((uint16_t)DoipPayloadType::VehicleIdentificationRequest, 0x0001);
  EXPECT_EQ((uint16_t)DoipPayloadType::VehicleAnnouncement, 0x0004);
  EXPECT_EQ((uint16_t)DoipPayloadType::RoutingActivationRequest, 0x0005);
  EXPECT_EQ((uint16_t)DoipPayloadType::RoutingActivationResponse, 0x0006);
  EXPECT_EQ((uint16_t)DoipPayloadType::DiagnosticMessage, 0x8001);
}

TEST(DoipTypesTest, RoutingActivationCodeValues) {
  EXPECT_EQ((uint8_t)RoutingActivationCode::RoutingActivated, 0x10);
  EXPECT_EQ((uint8_t)RoutingActivationCode::UnknownSourceAddress, 0x00);
}

TEST(DoipTypesTest, DoipMessageConstruction) {
  DoipMessage msg;
  msg.payload_type = 0x8001;
  msg.source_address = 0x0E00;
  msg.target_address = 0x0001;
  msg.payload = {0x10, 0x03};

  EXPECT_EQ(msg.payload_type, 0x8001);
  EXPECT_EQ(msg.source_address, 0x0E00);
  EXPECT_EQ(msg.target_address, 0x0001);
  ASSERT_EQ(msg.payload.size(), 2);
  EXPECT_EQ(msg.payload[0], 0x10);
  EXPECT_EQ(msg.payload[1], 0x03);
}

TEST(DoipTypesTest, EcuInfoDefaults) {
  EcuInfo ecu;
  EXPECT_EQ(ecu.source_address, "");
  EXPECT_EQ(ecu.vin, "");
  EXPECT_EQ(ecu.logical_address, 0);
  EXPECT_EQ(ecu.vin_sync_status, 0);
}
