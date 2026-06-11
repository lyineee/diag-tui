#include "uds/DtcDatabase.h"
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <sstream>

using json = nlohmann::json;

bool DtcDatabase::Load(const std::string& filepath) {
  std::ifstream file(filepath);
  if (!file.is_open()) {
    spdlog::error("Failed to open DTC database: {}", filepath);
    return false;
  }
  std::stringstream ss;
  ss << file.rdbuf();
  try {
    auto j = json::parse(ss.str());
    entries_.clear();
    for (const auto& item : j) {
      DtcEntry entry;
      entry.code = item["dtc"].get<std::string>();
      entry.name = item.value("name", "");
      entry.description = item.value("description", "");
      entries_.push_back(entry);
    }
    spdlog::info("Loaded {} DTC entries", entries_.size());
    return true;
  } catch (const std::exception& e) {
    spdlog::error("Failed to parse DTC database JSON: {}", e.what());
    return false;
  }
}

DtcEntry DtcDatabase::Find(const std::string& code) const {
  std::string upper = code;
  std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
  for (const auto& e : entries_) {
    if (e.code == upper) return e;
  }
  return DtcEntry{};
}

std::vector<DtcEntry> DtcDatabase::GetAll() const { return entries_; }

DtcDatabase& DtcDatabase::Instance() {
  static DtcDatabase instance;
  return instance;
}
