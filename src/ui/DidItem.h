#pragma once

#include "app/App.h"
#include "uds/DidDatabase.h"
#include <ftxui/component/component.hpp>
#include <string>

class DidItem {
public:
  DidItem(App& app, const DidEntry& entry, bool* expanded);
  ftxui::Component Build();

private:
  ftxui::Component BuildContent();

  App& app_;
  DidEntry entry_;
  bool* expanded_;
  std::string label_;
  ftxui::Component content_;
};
