#pragma once

#include "app/App.h"
#include "ui/settings/ConnectionPanel.h"
#include "ui/settings/EcuDiscoveryPanel.h"
#include <ftxui/component/component.hpp>
#include <memory>
#include <string>

class SettingsPage {
public:
  explicit SettingsPage(App& app);
  ftxui::Component Build();

private:
  App& app_;
  ConnectionPanel connection_;
  std::shared_ptr<EcuDiscoveryPanel> ecu_panel_;
  ftxui::Component conn_component_;
  ftxui::Component ecu_component_;
  ftxui::Component renderer_;
};
