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
  App& app_;
  DidEntry entry_;
  bool* expanded_;
  std::string label_;
  bool graph_checkbox_{false};
};
