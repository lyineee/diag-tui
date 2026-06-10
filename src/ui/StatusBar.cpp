#include "ui/StatusBar.h"
#include "app/App.h"
#include <ftxui/dom/elements.hpp>
#include <spdlog/spdlog.h>
#include <sstream>

using namespace ftxui;

StatusBar::StatusBar(App& app) : app_(app) {
  renderer_ = Renderer(std::function<Element()>([this] {
    auto& state = app_.GetState();
    std::lock_guard<std::mutex> lock(state.mtx);

    Elements left;
    left.push_back(text(" FuseDiag v0.1 "));

    if (state.connecting) {
      left.push_back(text(" Connecting... ") | color(Color::YellowLight));
    } else if (state.connected) {
      auto ip = text(" " + state.connected_ip + " ");
      left.push_back(ip | color(Color::GreenLight));
    } else {
      left.push_back(text(" Disconnected ") | color(Color::RedLight));
    }

    if (state.routing_ok) {
      left.push_back(text(" [RA] ") | color(Color::GreenLight));
    }

    std::stringstream ss;
    ss << " SA:0x" << std::hex << std::uppercase << state.config_source_addr
       << " TA:0x" << state.config_target_addr;
    left.push_back(text(" " + ss.str() + " "));

    left.push_back(text(" " + state.session_name + " "));

    Element status = hbox(std::move(left));
    Element bar = status | bgcolor(Color::Blue) | bold | size(WIDTH, GREATER_THAN, 0);

    return bar;
  }));
}

ftxui::Component StatusBar::Build() { return renderer_; }

void StatusBar::Update() {
  auto& state = app_.GetState();
  std::lock_guard<std::mutex> lock(state.mtx);
  connected_ = state.connected;
}
