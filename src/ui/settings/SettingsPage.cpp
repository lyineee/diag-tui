#include "ui/settings/SettingsPage.h"
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>

using namespace ftxui;

SettingsPage::SettingsPage(App& app) : app_(app), connection_(app) {
  ecu_panel_ = std::make_shared<EcuDiscoveryPanel>(app_);
}

Component SettingsPage::Build() {
  conn_component_ = connection_.Build();
  ecu_component_ = ecu_panel_->Build();
  ecu_panel_->SetOnSelectEcu([this](const std::string& ip) {
    auto& state = app_.GetState();
    std::lock_guard<std::recursive_mutex> lock(state.mtx);
    state.config_ip = ip;
  });

  auto container = Container::Vertical({conn_component_, ecu_component_});

  renderer_ = Renderer(container, [this] {
    auto& state = app_.GetState();
    std::lock_guard<std::recursive_mutex> lock(state.mtx);
    std::string status;
    if (state.connecting) status = "Connecting to " + state.config_ip + "...";
    else if (state.connected) {
      status = "Connected to " + state.connected_ip;
      if (state.routing_ok) status += " (routing active)";
      else status += " (routing pending)";
    } else status = "Disconnected";
    if (!state.status_message.empty()) status += "\n" + state.status_message;
    return vbox({
      conn_component_->Render(),
      window(text(" Status "), paragraph(status) | flex),
      ecu_component_->Render() | flex,
    }) | flex;
  });

  return renderer_;
}
