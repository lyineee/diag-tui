#include "ui/settings/EcuDiscoveryPanel.h"
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>

#include <sstream>

using namespace ftxui;

EcuDiscoveryPanel::EcuDiscoveryPanel(App& app) : app_(app) {}

void EcuDiscoveryPanel::SetOnSelectEcu(std::function<void(const std::string& ip)> cb) {
  on_select_ecu_ = std::move(cb);
}

Component EcuDiscoveryPanel::Build() {
  return Renderer([this](bool) -> Element {
    auto& state = app_.GetState();
    std::lock_guard<std::recursive_mutex> lock(state.mtx);
    const auto& ecus = state.discovered_ecus;
    int count = (int)ecus.size();
    if (selected_ >= count) selected_ = count > 0 ? count - 1 : 0;

    Elements items;
    for (int i = 0; i < count; i++) {
      const auto& ecu = ecus[i];
      std::stringstream ss;
      ss << "VIN:" << (ecu.vin.empty() ? "???" : ecu.vin)
         << "  LA:0x" << std::hex << std::uppercase << ecu.logical_address;
      auto dot = text(i == selected_ ? " > " : "   ");
      auto row = hbox({dot, text(ss.str())});
      if (i == selected_) row = row | inverted;
      items.push_back(row);
    }
    if (items.empty()) {
      if (state.discovering) items.push_back(text(" Discovering... "));
      else items.push_back(text(" No ECUs discovered "));
    }
    return window(text(" Discovered ECUs "),
      vbox(std::move(items)) | vscroll_indicator | yframe | flex);
  }) | CatchEvent([this](Event event) {
    auto& state = app_.GetState();
    std::lock_guard<std::recursive_mutex> lock(state.mtx);
    int count = (int)state.discovered_ecus.size();
    if (event == Event::ArrowDown || event == Event::Character('j')) {
      if (selected_ + 1 < count) { selected_++; return true; }
    }
    if (event == Event::ArrowUp || event == Event::Character('k')) {
      if (selected_ > 0) { selected_--; return true; }
    }
    if (event == Event::Return) {
      if (selected_ >= 0 && selected_ < count) {
        if (on_select_ecu_) on_select_ecu_(state.discovered_ecus[selected_].source_address);
        return true;
      }
    }
    if (event.is_mouse()) {
      if (event.mouse().button == Mouse::WheelDown && selected_ + 1 < count)
        { selected_++; return true; }
      if (event.mouse().button == Mouse::WheelUp && selected_ > 0)
        { selected_--; return true; }
    }
    return false;
  });
}
