#pragma once

#include "app/App.h"
#include <ftxui/component/component.hpp>

class EcuDiscoveryPanel {
public:
  explicit EcuDiscoveryPanel(App& app);
  ftxui::Component Build();
  void SetOnSelectEcu(std::function<void(const std::string& ip)> cb);

private:
  App& app_;
  std::function<void(const std::string&)> on_select_ecu_;
  int selected_{0};
};
