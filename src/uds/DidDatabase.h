#pragma once

#include "uds/UdsTypes.h"
#include <string>
#include <vector>

class DidDatabase {
public:
  bool Load(const std::string& filepath);
  bool LoadFromJson(const std::string& json_str);
  bool Save(const std::string& filepath);

  DidEntry Find(uint16_t did) const;
  std::vector<DidEntry> Search(const std::string& keyword) const;
  std::vector<DidEntry> GetAll() const;
  void SetGraphable(uint16_t did, bool value);
  void SetExpanded(uint16_t did, bool value);
  bool GetExpanded(uint16_t did) const;

  int GetPollInterval() const;
  void SetPollInterval(int sec);

  static DidDatabase& Instance();

private:
  std::vector<DidEntry> entries_;
  int polling_interval_{3};
};
