#include <gtest/gtest.h>
#include "uds/UdsMessage.h"
#include "uds/DidDatabase.h"
#include <cstring>
#include <vector>

TEST(UdsMessageTest, BuildSessionControlRequest) {
  auto req = UdsMessage::MakeRequest(0x10, 0x03, true, 1);
  auto bytes = UdsMessage::BuildRequest(req);
  ASSERT_EQ(bytes.size(), 2);
  EXPECT_EQ(bytes[0], 0x10);
  EXPECT_EQ(bytes[1], 0x03);
}

TEST(UdsMessageTest, BuildReadDidRequest) {
  auto req = UdsMessage::MakeRequest(0x22, 0xF190, true, 2);
  auto bytes = UdsMessage::BuildRequest(req);
  ASSERT_EQ(bytes.size(), 3);
  EXPECT_EQ(bytes[0], 0x22);
  EXPECT_EQ(bytes[1], 0xF1);
  EXPECT_EQ(bytes[2], 0x90);
}

TEST(UdsMessageTest, BuildReadDtcRequest) {
  uint8_t payload[] = {0x00, 0xFF};
  auto req = UdsMessage::MakeRequest(0x19, 0x02, true, 1, payload, 2);
  auto bytes = UdsMessage::BuildRequest(req);
  ASSERT_EQ(bytes.size(), 4);
  EXPECT_EQ(bytes[0], 0x19);
  EXPECT_EQ(bytes[1], 0x02);
  EXPECT_EQ(bytes[2], 0x00);
  EXPECT_EQ(bytes[3], 0xFF);
}

TEST(UdsMessageTest, ParsePositiveResponse) {
  std::vector<uint8_t> raw = {0x50, 0x03};
  auto resp = UdsMessage::ParseResponse(raw);
  EXPECT_TRUE(resp.success);
  EXPECT_EQ(resp.mode, 0x10);
  ASSERT_EQ(resp.payload_length, 1);
  EXPECT_EQ(resp.payload[0], 0x03);
}

TEST(UdsMessageTest, ParseNegativeResponse) {
  std::vector<uint8_t> raw = {0x7F, 0x22, 0x31};
  auto resp = UdsMessage::ParseResponse(raw);
  EXPECT_FALSE(resp.success);
  EXPECT_EQ(resp.mode, 0x22);
  EXPECT_EQ(resp.negative_response_code, NRC_REQUEST_OUT_OF_RANGE);
}

TEST(UdsMessageTest, ParseEmptyResponse) {
  std::vector<uint8_t> raw;
  auto resp = UdsMessage::ParseResponse(raw);
  EXPECT_FALSE(resp.success);
}

TEST(UdsMessageTest, BuildResponse) {
  auto pos = UdsMessage::BuildResponse(0x22, (const uint8_t*)"\xF1\x90\x01\x02", 4);
  ASSERT_EQ(pos.size(), 5);
  EXPECT_EQ(pos[0], 0x62);
  EXPECT_EQ(pos[1], 0xF1);
  EXPECT_EQ(pos[2], 0x90);
  EXPECT_EQ(pos[3], 0x01);
  EXPECT_EQ(pos[4], 0x02);
}

TEST(UdsMessageTest, BuildNegativeResponse) {
  auto neg = UdsMessage::BuildNegativeResponse(0x22, NRC_REQUEST_OUT_OF_RANGE);
  ASSERT_EQ(neg.size(), 3);
  EXPECT_EQ(neg[0], 0x7F);
  EXPECT_EQ(neg[1], 0x22);
  EXPECT_EQ(neg[2], 0x31);
}

TEST(UdsMessageTest, ParseRequest) {
  std::vector<uint8_t> raw = {0x22, 0xF1, 0x90};
  auto req = UdsMessage::ParseRequest(raw);
  EXPECT_EQ(req.mode, 0x22);
  EXPECT_TRUE(req.has_pid);
  EXPECT_EQ(req.pid, 0xF190);
  EXPECT_EQ(req.pid_length, 2);
}

TEST(UdsMessageTest, NrcToString) {
  EXPECT_EQ(UdsMessage::NrcToString(NRC_SUCCESS), "PositiveResponse");
  EXPECT_EQ(UdsMessage::NrcToString(NRC_SERVICE_NOT_SUPPORTED), "ServiceNotSupported");
  EXPECT_EQ(UdsMessage::NrcToString(NRC_REQUEST_OUT_OF_RANGE), "RequestOutOfRange");
}

TEST(DidDatabaseTest, LoadFromJson) {
  DidDatabase db;
  std::string json = R"([
    {"did": "F190", "name": "VIN", "description": "VIN number", "data_size": 17},
    {"did": "F1A0", "name": "SW Version", "description": "Software version", "data_size": 8}
  ])";

  EXPECT_TRUE(db.LoadFromJson(json));

  auto entry = db.Find(0xF190);
  EXPECT_EQ(entry.did, 0xF190);
  EXPECT_EQ(entry.name, "VIN");

  entry = db.Find(0xFFFF);
  EXPECT_EQ(entry.did, 0);
}

TEST(DidDatabaseTest, SearchByName) {
  DidDatabase db;
  std::string json = R"([
    {"did": "F190", "name": "VIN", "description": "VIN number", "data_size": 17},
    {"did": "F1A0", "name": "ECU Software Version", "description": "SW ver", "data_size": 8}
  ])";
  db.LoadFromJson(json);

  auto results = db.Search("software");
  ASSERT_EQ(results.size(), 1);
  EXPECT_EQ(results[0].did, 0xF1A0);
}
