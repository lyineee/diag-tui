#include "uds/DidDatabase.h"
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <sstream>

using json = nlohmann::json;

bool DidDatabase::Load(const std::string& filepath) {
  std::ifstream file(filepath);
  if (!file.is_open()) {
    spdlog::error("Failed to open DID database: {}", filepath);
    return false;
  }

  std::stringstream ss;
  ss << file.rdbuf();
  return LoadFromJson(ss.str());
}

bool DidDatabase::LoadFromJson(const std::string& json_str) {
  try {
    auto j = json::parse(json_str);
    entries_.clear();

    for (const auto& item : j) {
      DidEntry entry;
      entry.did = std::stoul(item["did"].get<std::string>(), nullptr, 16);
      entry.name = item.value("name", "");
      entry.description = item.value("description", "");
      entry.data_size = item.value("data_size", 0);
      entry.graphable = item.value("graphable", false);
      entries_.push_back(entry);
    }

    spdlog::info("Loaded {} DID entries", entries_.size());
    return true;
  } catch (const std::exception& e) {
    spdlog::error("Failed to parse DID database JSON: {}", e.what());
    return false;
  }
}

DidEntry DidDatabase::Find(uint16_t did) const {
  auto it = std::find_if(entries_.begin(), entries_.end(),
                          [did](const DidEntry& e) { return e.did == did; });
  if (it != entries_.end()) return *it;
  return DidEntry{};
}

std::vector<DidEntry> DidDatabase::Search(const std::string& keyword) const {
  std::vector<DidEntry> results;
  std::string lower = keyword;
  std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

  for (const auto& e : entries_) {
    std::string name_lower = e.name;
    std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(),
                   ::tolower);
    if (name_lower.find(lower) != std::string::npos) {
      results.push_back(e);
    }
  }

  return results;
}

std::vector<DidEntry> DidDatabase::GetAll() const {
  return entries_;
}

DidDatabase& DidDatabase::Instance() {
  static DidDatabase instance;
  return instance;
}
