#include "ui/dtc/DtcListPanel.h"
#include "uds/DtcDatabase.h"
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>

#include <sstream>
#include <iomanip>

using namespace ftxui;

static std::string MaskHex(int m) {
  std::stringstream ss;
  ss << std::hex << m;
  return ss.str();
}

DtcListPanel::DtcListPanel(App& app) : app_(app) {}

void DtcListPanel::Refresh() { app_.ReadDtc(); }

Component DtcListPanel::Build() {
  return Renderer([this](bool) -> Element {
    auto& state = app_.GetState();
    std::lock_guard<std::recursive_mutex> lock(state.mtx);
    const auto& dtcs = state.dtc_list;
    int count = (int)dtcs.size();
    if (selected_ >= count) selected_ = count > 0 ? count - 1 : 0;

    mask_.mask = state.dtc_status_mask;

    Elements dtc_els;
    for (int i = 0; i < count; i++) {
      const auto& dtc = dtcs[i];
      auto code = dtc.CodeStr();
      auto entry = DtcDatabase::Instance().Find(code);

      auto dot = text(i == selected_ ? " \u25B8 " : "   ");
      auto line = text(code) | bold;
      Elements dots;
      auto mk = [&](uint8_t m, Color c) {
        dots.push_back(text(dtc.status & m ? "\u25CF" : "\u25CB") | color(c));
      };
      mk(0x01, Color::RedLight);
      mk(0x08, Color::YellowLight);
      mk(0x04, Color::Orange1);
      mk(0x20, Color::Cyan);

      auto name_el = text(" ") | size(WIDTH, EQUAL, 0);
      if (!entry.name.empty()) name_el = text(" " + entry.name) | dim;

      auto row = hbox({dot, line, separator(), hbox(std::move(dots)),
                       separator(), name_el | flex});
      if (i == selected_) row = row | inverted;
      dtc_els.push_back(row);
    }
    if (dtc_els.empty())
      dtc_els.push_back(text(" No DTCs found "));

    auto list_el = window(
        text(" DTC List (" + std::to_string(count) + " DTCs) (F5:Refresh) "),
        vbox(std::move(dtc_els)) | vscroll_indicator | yframe) | flex;

    Element detail_el;
    if (selected_ >= 0 && selected_ < count) {
      const auto& dtc = dtcs[selected_];
      auto code = dtc.CodeStr();
      auto entry = DtcDatabase::Instance().Find(code);

      Elements rows;
      rows.push_back(hbox({
          text(" DTC ") | bold,
          text(" " + code + " ") | color(Color::Cyan) | bold,
      }));
      if (!entry.name.empty())
        rows.push_back(hbox({text(" Name: ") | bold, text(entry.name) | dim}));
      if (!entry.description.empty())
        rows.push_back(hbox({text(" Desc: ") | bold,
          paragraph(" " + entry.description) | dim | size(WIDTH, EQUAL, 28)}));
      rows.push_back(separator());

      auto bit_row = [&](const char* label, uint8_t m, Color c) {
        auto val = text(dtc.status & m ? " Yes " : " No ");
        if (dtc.status & m) val = val | color(c) | bold;
        return hbox({text("  ") | dim, text(label) | bold,
                     separator() | flex, val});
      };
      rows.push_back(bit_row("testFailed",                0x01, Color::RedLight));
      rows.push_back(bit_row("testFailedThisOpCycle",     0x02, Color::Red));
      rows.push_back(bit_row("pendingDTC",                0x04, Color::Orange1));
      rows.push_back(bit_row("confirmedDTC",              0x08, Color::YellowLight));
      rows.push_back(bit_row("testNotCompleteSinceClr",   0x10, Color::GrayDark));
      rows.push_back(bit_row("testFailedSinceClear",      0x20, Color::Cyan));
      rows.push_back(bit_row("testNotCompleteThisCycle",  0x40, Color::Yellow));
      rows.push_back(bit_row("warningIndicatorRequested", 0x80, Color::Red));
      rows.push_back(separator());
      rows.push_back(hbox({text(" Raw:  0x"), text(std::to_string(dtc.dtc_number)) | dim}));
      rows.push_back(hbox({text(" Stat: 0x"), text(std::to_string(dtc.status)) | dim}));
      rows.push_back(separator());
      rows.push_back(text(" Legend ") | bold);
      rows.push_back(hbox({text("  ") | dim, text("\u25CF") | color(Color::RedLight),    text(" testFailed")}));
      rows.push_back(hbox({text("  ") | dim, text("\u25CF") | color(Color::YellowLight), text(" confirmedDTC")}));
      rows.push_back(hbox({text("  ") | dim, text("\u25CF") | color(Color::Orange1),     text(" pendingDTC")}));
      rows.push_back(hbox({text("  ") | dim, text("\u25CF") | color(Color::Cyan),        text(" testFailedSinceClear")}));

      detail_el = window(text(" DTC Detail "), vbox(std::move(rows))) | size(WIDTH, EQUAL, 38);
    } else {
      detail_el = window(text(" DTC Detail "),
                         text(" Select a DTC from the list ")) | size(WIDTH, EQUAL, 38);
    }

    auto footer = text(" " + std::to_string(count) + " DTCs  |  Count: "
      + std::to_string(state.dtc_count) + "  |  Mask: 0x" + MaskHex(mask_.mask)
      + "  |  F6:Clear ") | dim;

    Elements main_body = {mask_.Render(), separator(),
      hbox({list_el, separator(), detail_el}) | flex,
      separator(), footer};
    return vbox(std::move(main_body)) | flex;
  }) | CatchEvent([this](Event event) {
    if (mask_.expanded) {
      auto& state = app_.GetState();
      bool handled = mask_.HandleEvent(event, state.dtc_status_mask);
      if (handled) {
        state.dtc_status_mask = mask_.mask;
        if (!mask_.expanded) Refresh();
        return true;
      }
      return false;
    }

    if (event == Event::Character('m'))
      { mask_.expanded = true; return true; }
    if (event == Event::Character('j') || event == Event::ArrowDown) {
      auto& state = app_.GetState();
      std::lock_guard<std::recursive_mutex> lock(state.mtx);
      if (selected_ + 1 < (int)state.dtc_list.size()) { selected_++; return true; }
    }
    if (event == Event::Character('k') || event == Event::ArrowUp) {
      if (selected_ > 0) { selected_--; return true; }
    }
    if (event == Event::F5) { app_.ReadDtc(); return true; }
    if (event == Event::F6) { app_.ClearDtc(); return true; }
    return false;
  });
}
