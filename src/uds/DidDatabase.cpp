#include "uds/DidDatabase.h"
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <iomanip>
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

    // Accept both object format {polling_interval, dids[]} and legacy array []
    json arr;
    if (j.is_object()) {
      polling_interval_ = j.value("polling_interval", 3);
      arr = j["dids"];
    } else {
      polling_interval_ = 3;
      arr = j;
    }

    for (const auto& item : arr) {
      DidEntry entry;
      entry.did = std::stoul(item["did"].get<std::string>(), nullptr, 16);
      entry.name = item.value("name", "");
      entry.description = item.value("description", "");
      entry.data_size = item.value("data_size", 0);
      entry.graphable = item.value("graphable", false);
      entry.expanded = item.value("expanded", false);
      entries_.push_back(entry);
    }

    spdlog::info("Loaded {} DID entries, interval={}", entries_.size(), polling_interval_);
    return true;
  } catch (const std::exception& e) {
    spdlog::error("Failed to parse DID database JSON: {}", e.what());
    return false;
  }
}

bool DidDatabase::Save(const std::string& filepath) {
  try {
    json arr = json::array();
    for (const auto& e : entries_) {
      json item;
      std::stringstream ss;
      ss << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << e.did;
      item["did"] = ss.str();
      item["name"] = e.name;
      item["description"] = e.description;
      item["data_size"] = e.data_size;
      if (e.graphable) item["graphable"] = true;
      if (e.expanded) item["expanded"] = true;
      arr.push_back(item);
    }
    json root;
    root["polling_interval"] = polling_interval_;
    root["dids"] = arr;

    std::ofstream file(filepath);
    if (!file.is_open()) {
      spdlog::error("Failed to save DID database: {}", filepath);
      return false;
    }
    file << root.dump(2) << std::endl;
    return true;
  } catch (const std::exception& e) {
    spdlog::error("Failed to save DID database: {}", e.what());
    return false;
  }
}

DidEntry DidDatabase::Find(uint16_t did) const {
  auto it = std::find_if(entries_.begin(), entries_.end(),
                          [did](const DidEntry& e) { return e.did == did; });
  if (it != entries_.end()) return *it;
  return DidEntry{};
}

void DidDatabase::SetGraphable(uint16_t did, bool value) {
  for (auto& e : entries_) {
    if (e.did == did) { e.graphable = value; return; }
  }
}

void DidDatabase::SetExpanded(uint16_t did, bool value) {
  for (auto& e : entries_) {
    if (e.did == did) { e.expanded = value; return; }
  }
}

bool DidDatabase::GetExpanded(uint16_t did) const {
  for (auto& e : entries_) {
    if (e.did == did) return e.expanded;
  }
  return false;
}

int DidDatabase::GetPollInterval() const { return polling_interval_; }

void DidDatabase::SetPollInterval(int sec) { polling_interval_ = sec; }

std::vector<DidEntry> DidDatabase::Search(const std::string& keyword) const {
  std::vector<DidEntry> results;
  std::string lower = keyword;
  std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
  for (const auto& e : entries_) {
    std::string name_lower = e.name;
    std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
    if (name_lower.find(lower) != std::string::npos)
      results.push_back(e);
  }
  return results;
}

std::vector<DidEntry> DidDatabase::GetAll() const { return entries_; }

DidDatabase& DidDatabase::Instance() {
  static DidDatabase instance;
  return instance;
}
