#include "ui/SettingsPage.h"
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>
#include <spdlog/spdlog.h>

#include <iomanip>
#include <sstream>

using namespace ftxui;

SettingsPage::SettingsPage(App& app) : app_(app) {
  auto& state = app.GetState();
  std::lock_guard<std::recursive_mutex> lock(state.mtx);
  ip_input_ = state.config_ip;
  std::stringstream ss;
  ss << std::hex << state.config_source_addr;
  src_input_ = ss.str();
  ss.str("");
  ss << std::hex << state.config_target_addr;
  tgt_input_ = ss.str();
}

void SettingsPage::DoConnect() {
  auto& state = app_.GetState();
  {
    std::lock_guard<std::recursive_mutex> lock(state.mtx);
    state.config_ip = ip_input_;
    try {
      state.config_source_addr = (uint16_t)std::stoul(src_input_, nullptr, 16);
      state.config_target_addr = (uint16_t)std::stoul(tgt_input_, nullptr, 16);
    } catch (...) {}
  }
  app_.ConnectWithConfig();
}

void SettingsPage::DoDiscover() {
  app_.StartUdpDiscovery();
}

void SettingsPage::SelectEcu(int index) {
  auto& state = app_.GetState();
  std::lock_guard<std::recursive_mutex> lock(state.mtx);
  if (index >= 0 && (size_t)index < state.discovered_ecus.size()) {
    ip_input_ = state.discovered_ecus[index].source_address;
    selected_ecu_ = index;
  }
}

ftxui::Component SettingsPage::Build() {
  input_ip_ = Input(&ip_input_, " e.g. 127.0.0.1");
  input_src_ = Input(&src_input_, " hex e.g. e00");
  input_tgt_ = Input(&tgt_input_, " hex e.g. e80");

  btn_connect_ = Button("Connect", [this] { DoConnect(); });
  btn_disconnect_ = Button("Disconnect", [this] { app_.Disconnect(); });
  btn_discover_ = Button("UDP Discover", [this] { DoDiscover(); });

  auto container = Container::Vertical({
      input_ip_,
      input_src_,
      input_tgt_,
      btn_connect_,
      btn_disconnect_,
      btn_discover_,
  });

  renderer_ = Renderer(container, std::function<Element()>([this] {
    auto& state = app_.GetState();
    std::lock_guard<std::recursive_mutex> lock(state.mtx);

    std::string status;
    if (state.connecting) {
      status = "Connecting to " + state.config_ip + "...";
    } else if (state.connected) {
      status = "Connected to " + state.connected_ip;
      if (state.routing_ok) status += " (routing active)";
      else status += " (routing pending)";
    } else {
      status = "Disconnected";
    }
    if (!state.status_message.empty()) {
      status += "\n" + state.status_message;
    }

    Elements ecu_items;
    for (size_t i = 0; i < state.discovered_ecus.size(); i++) {
      const auto& ecu = state.discovered_ecus[i];
      std::stringstream ss;
      ss << "VIN:" << (ecu.vin.empty() ? "???" : ecu.vin)
         << "  LA:0x" << std::hex << std::uppercase << ecu.logical_address;
      auto el = text(ss.str());
      if ((int)i == selected_ecu_) el = el | bold | color(Color::Cyan);
      ecu_items.push_back(el);
    }
    if (ecu_items.empty()) {
      if (state.discovering) {
        ecu_items.push_back(text(" Discovering... "));
      } else {
        ecu_items.push_back(text(" No ECUs discovered "));
      }
    }

    return vbox({
               window(text(" Connection Settings "),
                      vbox({
                          hbox({text(" IP: ") | bold, input_ip_->Render() | flex}),
                          separator(),
                          hbox({text(" Source Addr (hex): ") | bold, input_src_->Render() | flex}),
                          separator(),
                          hbox({text(" Target Addr (hex): ") | bold, input_tgt_->Render() | flex}),
                          separator(),
                          hbox({btn_connect_->Render(), separator(), btn_disconnect_->Render()}),
                          separator(),
                          hbox({btn_discover_->Render()}),
                      })) | size(WIDTH, EQUAL, 60),
               window(text(" Status "), paragraph(status) | flex),
               window(text(" Discovered ECUs "), vbox(std::move(ecu_items)) | vscroll_indicator | yframe | flex),
           }) |
           flex;
  }));

  return renderer_;
}
