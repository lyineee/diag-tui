#include "ui/dtc/DtcSnapshotPanel.h"
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>

#include <sstream>
#include <iomanip>

using namespace ftxui;

static std::string HexDump(const std::vector<uint8_t>& data) {
  if (data.empty()) return "(empty)";
  std::stringstream ss;
  ss << std::hex << std::uppercase << std::setfill('0');
  for (size_t i = 0; i < data.size(); i++) {
    ss << std::setw(2) << (int)data[i] << " ";
    if ((i + 1) % 16 == 0 && i + 1 < data.size()) ss << "\n";
  }
  return ss.str();
}

DtcSnapshotPanel::DtcSnapshotPanel(App& app) : app_(app) {}

void DtcSnapshotPanel::Refresh() { app_.ReadDtcSnapshots(); }

Component DtcSnapshotPanel::Build() {
  auto btn_refresh = Button("Refresh (F5)", [this] { Refresh(); }, ButtonOption::Ascii());
  auto btn_bar = Container::Horizontal({btn_refresh});
  auto btn_bar_wrap = Renderer(btn_bar, [] { return text(""); });

  auto list_renderer = Renderer([this, btn_refresh](bool) -> Element {
    auto& state = app_.GetState();
    std::lock_guard<std::recursive_mutex> lock(state.mtx);
    const auto& dtcs = state.snapshot_list;
    int count = (int)dtcs.size();
    if (selected_ >= count) selected_ = count > 0 ? count - 1 : 0;

    Elements dtc_els;
    for (int i = 0; i < count; i++) {
      const auto& dtc = dtcs[i];
      auto code = dtc.CodeStr();
      auto dot = text(i == selected_ ? " > " : "   ");
      auto line = text(code) | bold;
      auto cnt = text(" snap:" + std::to_string(dtc.snapshot_count)) | dim;
      auto row = hbox({dot, line, separator(), cnt | flex});
      if (i == selected_) row = row | inverted;
      dtc_els.push_back(row);
    }
    if (dtc_els.empty())
      dtc_els.push_back(text(" No snapshots found "));

    auto title = " Snapshots (" + std::to_string(count) + " DTCs) ";
    auto list_el = window(text(title), vbox(std::move(dtc_els)) | vscroll_indicator | yframe) | flex | reflect(list_box_);

    Element detail_el;
    if (selected_ >= 0 && selected_ < count) {
      const auto& dtc = dtcs[selected_];
      Elements rows;
      rows.push_back(hbox({text(" DTC ") | bold, text(" " + dtc.CodeStr() + " ") | color(Color::Cyan) | bold}));
      rows.push_back(hbox({text(" Status: 0x"), text(std::to_string(dtc.status)) | dim}));
      rows.push_back(hbox({text(" Snapshots: "), text(std::to_string(dtc.snapshot_count)) | bold}));
      rows.push_back(separator());

      if (!state.selected_snapshot_data.empty()) {
        rows.push_back(text(" Snapshot Data:") | bold);
        rows.push_back(text(" " + HexDump(state.selected_snapshot_data)) | dim);
      } else {
        rows.push_back(text(" Enter: fetch snapshot record ") | dim);
      }
      detail_el = window(text(" Snapshot Detail "), vbox(std::move(rows))) | size(WIDTH, EQUAL, 40);
    } else {
      detail_el = window(text(" Snapshot Detail "), text(" Select a DTC from the list ")) | size(WIDTH, EQUAL, 40);
    }

    auto status = text(" " + std::to_string(count) + " DTC(s) with snapshots ") | dim;

    Elements body;
    body.push_back(hbox({list_el, separator(), detail_el}) | flex);
    body.push_back(separator());
    body.push_back(hbox({btn_refresh->Render(), separator() | flex, status}));
    return vbox(std::move(body)) | flex;
  }) | CatchEvent([this](Event event) {
    if (event.is_mouse()) {
      if (event.mouse().button == Mouse::WheelDown) {
        auto& state = app_.GetState();
        std::lock_guard<std::recursive_mutex> lock(state.mtx);
        if (selected_ + 1 < (int)state.snapshot_list.size()) { selected_++; return true; }
      }
      if (event.mouse().button == Mouse::WheelUp) {
        if (selected_ > 0) { selected_--; return true; }
      }
      if (event.mouse().button == Mouse::Left && event.mouse().motion == Mouse::Released) {
        if (list_box_.Contain(event.mouse().x, event.mouse().y)) {
          int row = event.mouse().y - list_box_.y_min - 1;
          if (row >= 0) {
            auto& state = app_.GetState();
            std::lock_guard<std::recursive_mutex> lock(state.mtx);
            if (row < (int)state.snapshot_list.size()) { selected_ = row; return true; }
          }
        }
      }
      return false;
    }
    if (event == Event::ArrowDown || event == Event::Character('j')) {
      auto& state = app_.GetState();
      std::lock_guard<std::recursive_mutex> lock(state.mtx);
      if (selected_ + 1 < (int)state.snapshot_list.size()) { selected_++; return true; }
    }
    if (event == Event::ArrowUp || event == Event::Character('k')) {
      if (selected_ > 0) { selected_--; return true; }
    }
    if (event == Event::Return) {
      auto& state = app_.GetState();
      std::lock_guard<std::recursive_mutex> lock(state.mtx);
      if (selected_ >= 0 && selected_ < (int)state.snapshot_list.size()) {
        const auto& dtc = state.snapshot_list[selected_];
        app_.ReadSnapshotRecord(dtc.dtc_number, 1);
        return true;
      }
    }
    if (event == Event::F5) { Refresh(); return true; }
    return false;
  });

  Components children;
  children.push_back(list_renderer);
  children.push_back(btn_bar_wrap);
  container_ = Container::Vertical(std::move(children));
  return container_;
}
