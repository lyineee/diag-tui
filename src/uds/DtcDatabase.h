#pragma once

#include <string>
#include <vector>

struct DtcEntry {
  std::string code;  // e.g. "P0301"
  std::string name;
  std::string description;
};

class DtcDatabase {
public:
  bool Load(const std::string& filepath);
  DtcEntry Find(const std::string& code) const;
  std::vector<DtcEntry> GetAll() const;
  static DtcDatabase& Instance();

private:
  std::vector<DtcEntry> entries_;
};
