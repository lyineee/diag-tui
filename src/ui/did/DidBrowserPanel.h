#pragma once

#include "app/App.h"
#include <ftxui/component/component.hpp>

class DidBrowserPanel {
public:
  explicit DidBrowserPanel(App& app);
  ftxui::Component Build();

private:
  void DoWrite();
  App& app_;
  std::string did_input_;
  std::string data_input_;
  uint16_t last_read_did_{0};
};
