#pragma once

#include "uds/UdsTypes.h"
#include <string>
#include <vector>

class DidDatabase {
public:
  bool Load(const std::string& filepath);
  bool LoadFromJson(const std::string& json_str);

  DidEntry Find(uint16_t did) const;
  std::vector<DidEntry> Search(const std::string& keyword) const;
  std::vector<DidEntry> GetAll() const;

  static DidDatabase& Instance();

private:
  std::vector<DidEntry> entries_;
};
