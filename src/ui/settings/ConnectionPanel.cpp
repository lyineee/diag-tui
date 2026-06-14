#include "ui/settings/ConnectionPanel.h"
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>

#include <sstream>

using namespace ftxui;

ConnectionPanel::ConnectionPanel(App& app) : app_(app) {
  auto& state = app_.GetState();
  std::lock_guard<std::recursive_mutex> lock(state.mtx);
  ip_ = state.config_ip;
  std::stringstream ss;
  ss << std::hex << state.config_source_addr; src_ = ss.str(); ss.str("");
  ss << std::hex << state.config_target_addr; tgt_ = ss.str();
}

void ConnectionPanel::DoConnect() {
  auto& state = app_.GetState();
  { std::lock_guard<std::recursive_mutex> lock(state.mtx);
    state.config_ip = ip_;
    try {
      state.config_source_addr = (uint16_t)std::stoul(src_, nullptr, 16);
      state.config_target_addr = (uint16_t)std::stoul(tgt_, nullptr, 16);
    } catch (...) {}
  }
  app_.ConnectWithConfig();
}

void ConnectionPanel::DoDiscover() { app_.StartUdpDiscovery(); }

Component ConnectionPanel::Build() {
  auto input_ip = Input(&ip_, " e.g. 127.0.0.1");
  auto input_src = Input(&src_, " hex e.g. e00");
  auto input_tgt = Input(&tgt_, " hex e.g. e80");
  auto btn_connect = Button("Connect", [this] { DoConnect(); });
  auto btn_disconnect = Button("Disconnect", [this] { app_.Disconnect(); });
  auto btn_discover = Button("UDP Discover", [this] { DoDiscover(); });

  return Renderer(Container::Vertical({input_ip, input_src, input_tgt, btn_connect, btn_disconnect, btn_discover}),
    [this, input_ip, input_src, input_tgt, btn_connect, btn_disconnect, btn_discover] {
      return window(text(" Connection Settings "), vbox({
        hbox({text(" IP: ") | bold, input_ip->Render() | flex}),
        separator(),
        hbox({text(" Source Addr (hex): ") | bold, input_src->Render() | flex}),
        separator(),
        hbox({text(" Target Addr (hex): ") | bold, input_tgt->Render() | flex}),
        separator(),
        hbox({btn_connect->Render(), separator(), btn_disconnect->Render()}),
        separator(),
        hbox({btn_discover->Render()}),
      })) | size(WIDTH, EQUAL, 60);
    });
}
