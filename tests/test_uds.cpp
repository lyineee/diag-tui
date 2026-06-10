#include <gtest/gtest.h>
#include "uds/UdsClient.h"
#include "uds/UdsTypes.h"
#include "uds/DidDatabase.h"

#include <vector>

TEST(UdsTypesTest, SessionEnum) {
  EXPECT_EQ((uint8_t)DiagnosticSession::Default, 0x01);
  EXPECT_EQ((uint8_t)DiagnosticSession::Extended, 0x03);
  EXPECT_EQ((uint8_t)DiagnosticSession::Programming, 0x02);
}

TEST(UdsTypesTest, ResetTypeEnum) {
  EXPECT_EQ((uint8_t)EcuResetType::HardReset, 0x01);
  EXPECT_EQ((uint8_t)EcuResetType::SoftReset, 0x03);
}

TEST(UdsParseTest, PositiveResponse) {
  std::vector<uint8_t> raw = {0x50, 0x03};
  auto resp = UdsClient::ParseResponse(raw);

  EXPECT_TRUE(resp.success);
  EXPECT_EQ(resp.sid, 0x10);
  ASSERT_EQ(resp.data.size(), 1);
  EXPECT_EQ(resp.data[0], 0x03);
}

TEST(UdsParseTest, NegativeResponse) {
  std::vector<uint8_t> raw = {0x7F, 0x22, 0x31};
  auto resp = UdsClient::ParseResponse(raw);

  EXPECT_FALSE(resp.success);
  EXPECT_EQ(resp.sid, 0x22);
  EXPECT_EQ(resp.nrc, 0x31);
  EXPECT_EQ(resp.error_message, "RequestOutOfRange");
}

TEST(UdsParseTest, EmptyResponse) {
  std::vector<uint8_t> raw;
  auto resp = UdsClient::ParseResponse(raw);

  EXPECT_FALSE(resp.success);
  EXPECT_EQ(resp.error_message, "Empty response");
}

TEST(UdsParseTest, UdsDtcParse) {
  std::vector<uint8_t> raw = {0x59, 0x02, 0x01, 0x02, 0x03,
                                0x2F, 0x01, 0x02, 0x03, 0x2F};
  auto resp = UdsClient::ParseResponse(raw);

  EXPECT_TRUE(resp.success);
  EXPECT_EQ(resp.sid, 0x19);
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
