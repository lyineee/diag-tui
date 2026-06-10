#include "ui/StatusBar.h"
#include "app/App.h"
#include <ftxui/dom/elements.hpp>
#include <spdlog/spdlog.h>

using namespace ftxui;

StatusBar::StatusBar(App& app) : app_(app) {
  renderer_ = Renderer([this] {
    Update();
    auto& state = app_.GetState();
    std::lock_guard<std::mutex> lock(state.mtx);

    Elements left;
    left.push_back(text(" FuseDiag v0.1 "));

    if (state.connected) {
      auto ip = text(" Connected: " + state.connected_ip + " ");
      left.push_back(ip | color(Color::GreenLight));
    } else {
      left.push_back(text(" Disconnected ") | color(Color::RedLight));
    }

    left.push_back(text(" Session: " + state.session_name + " "));

    Element status = hbox(std::move(left));
    Element bar = status | bgcolor(Color::Blue) | bold | size(WIDTH, GREATER_THAN, 0);

    return bar;
  });
}

ftxui::Component StatusBar::Build() { return renderer_; }

void StatusBar::Update() {
  auto& state = app_.GetState();
  std::lock_guard<std::mutex> lock(state.mtx);
  connected_ = state.connected;
}
