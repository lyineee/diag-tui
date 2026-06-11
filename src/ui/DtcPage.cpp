#include "ui/DtcPage.h"
#include "uds/DtcDatabase.h"
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/screen/box.hpp>
#include <spdlog/spdlog.h>

#include <sstream>

using namespace ftxui;

DtcPage::DtcPage(App& app) : app_(app) {
  DtcDatabase::Instance().Load("config/dtc_database.json");
}

void DtcPage::Refresh() { app_.ReadDtc(); }

static std::string DtcCodeStr(uint32_t code) {
  uint8_t fb = (code >> 16) & 0xFF;
  uint16_t rest = code & 0xFFFF;
  char buf[16];
  snprintf(buf, sizeof(buf), "%c%04X", "PCBU"[(fb >> 4) & 3], rest);
  return std::string(buf);
}

ftxui::Component DtcPage::Build() {
  class Impl : public ComponentBase {
  public:
    Impl(App& app, int* selected) : app_(app), selected_(selected) {}

    Element Render() override {
      auto& state = app_.GetState();
      std::lock_guard<std::recursive_mutex> lock(state.mtx);

      const auto& data = state.last_dtc_response;
      int dtc_count = 0;

      // ── left panel: DTC list ──────────────────────────────────
      Elements dtc_els;

      if (data.size() >= 4) {
        size_t offset = 0;
        if (offset < data.size() && data[offset] == 0x02) offset++;
        if (offset < data.size()) offset++;

        int idx = 0;
        while (offset + 3 <= data.size()) {
          uint32_t code = ((uint32_t)data[offset] << 16) |
                          ((uint32_t)data[offset + 1] << 8) | data[offset + 2];
          uint8_t st = (offset + 3 < data.size()) ? data[offset + 3] : 0;

          auto code_str = DtcCodeStr(code);
          auto entry = DtcDatabase::Instance().Find(code_str);
          std::string dtc_name = entry.code.empty() ? "" : entry.name;

          auto dot = text(idx == *selected_ ? " ▸ " : "   ");
          auto line = text(code_str) | bold;
          Elements dots;
          auto mk = [&](uint8_t mask, Color c) {
            dots.push_back(text(st & mask ? "●" : "○") | color(c));
          };
          mk(0x01, Color::RedLight);
          mk(0x08, Color::YellowLight);
          mk(0x04, Color::Orange1);
          mk(0x20, Color::Cyan);

          auto name_el = text(" " + dtc_name) | dim;
          if (dtc_name.empty()) name_el = text("") | size(WIDTH, EQUAL, 0);

          auto row = hbox({dot, line, separator(), hbox(std::move(dots)),
                           separator(), name_el | flex});
          if (idx == *selected_) row = row | inverted;
          dtc_els.push_back(row);

          idx++;
          dtc_count = idx;
          offset += (offset + 4 <= data.size()) ? 4 : 3;
        }
      }

      if (*selected_ >= dtc_count) *selected_ = dtc_count > 0 ? dtc_count - 1 : 0;

      if (dtc_els.empty())
        dtc_els.push_back(text(" No DTCs found "));

      auto list_el = window(
          text(" DTC List (F5:Refresh F6:Clear \u2191\u2193:Select) "),
          vbox(std::move(dtc_els)) | vscroll_indicator | yframe) | flex;

      // ── right panel: DTC detail + legend ──────────────────────
      Element detail_el;
      if (*selected_ >= 0 && *selected_ < dtc_count && data.size() >= 4) {
        size_t offset = 0;
        if (offset < data.size() && data[offset] == 0x02) offset++;
        if (offset < data.size()) offset++;
        int idx = 0;
        uint32_t code = 0;
        uint8_t status = 0;
        while (offset + 3 <= data.size() && idx <= *selected_) {
          code = ((uint32_t)data[offset] << 16) |
                 ((uint32_t)data[offset + 1] << 8) | data[offset + 2];
          status = (offset + 3 < data.size()) ? data[offset + 3] : 0;
          idx++;
          offset += (offset + 4 <= data.size()) ? 4 : 3;
        }

        auto code_str = DtcCodeStr(code);
        auto entry = DtcDatabase::Instance().Find(code_str);

        Elements rows;
        rows.push_back(hbox({
            text(" DTC ") | bold,
            text(" " + code_str + " ") | color(Color::Cyan) | bold,
        }));
        if (!entry.name.empty())
          rows.push_back(hbox({text(" Name: ") | bold, text(entry.name) | dim}));
        if (!entry.description.empty())
          rows.push_back(hbox({text(" Desc: ") | bold, paragraph(" " + entry.description) | dim | size(WIDTH, EQUAL, 28)}));
        rows.push_back(separator());

        auto stat_row = [&](const char* label, uint8_t mask, Color on_color) {
          auto val = text(status & mask ? " Yes " : " No ");
          if (status & mask) val = val | color(on_color) | bold;
          return hbox({text("  ") | dim, text(label) | bold,
                       separator() | flex, val});
        };
        rows.push_back(stat_row("TestFailed",          0x01, Color::RedLight));
        rows.push_back(stat_row("TestFailedThisCycle", 0x20, Color::Red));
        rows.push_back(stat_row("TestThisCycle",       0x02, Color::Cyan));
        rows.push_back(stat_row("Pending",             0x04, Color::Orange1));
        rows.push_back(stat_row("Confirmed",           0x08, Color::YellowLight));
        rows.push_back(stat_row("TestNotSinceClear",   0x10, Color::GrayDark));
        rows.push_back(stat_row("WarningIndicator",    0x40, Color::Yellow));
        rows.push_back(separator());
        rows.push_back(hbox({text(" Raw:  0x"), text(std::to_string(code)) | dim}));
        rows.push_back(hbox({text(" Byte: 0x"), text(std::to_string(status)) | dim}));
        rows.push_back(separator());
        rows.push_back(text(" Legend ") | bold);
        rows.push_back(hbox({text("  ") | dim, text("●") | color(Color::RedLight),    text(" TestFailed")}));
        rows.push_back(hbox({text("  ") | dim, text("●") | color(Color::YellowLight), text(" Confirmed")}));
        rows.push_back(hbox({text("  ") | dim, text("●") | color(Color::Orange1),     text(" Pending")}));
        rows.push_back(hbox({text("  ") | dim, text("●") | color(Color::Cyan),        text(" ThisCycle")}));

        detail_el = window(text(" DTC Detail "), vbox(std::move(rows))) | size(WIDTH, EQUAL, 36);
      } else {
        detail_el = window(text(" DTC Detail "),
                           text(" Select a DTC from the list ")) | size(WIDTH, EQUAL, 36);
      }

      return hbox({list_el | flex,
                   separator(),
                   detail_el}) | flex | reflect(box_);
    }

    bool Focusable() const override { return true; }

    bool OnEvent(Event event) override {
      auto& state = app_.GetState();
      std::lock_guard<std::recursive_mutex> lock(state.mtx);
      const auto& data = state.last_dtc_response;
      int count = 0;
      if (data.size() >= 4) {
        size_t o = 0;
        if (o < data.size() && data[o] == 0x02) o++;
        if (o < data.size()) o++;
        while (o + 3 <= data.size()) { count++; o += (o + 4 <= data.size()) ? 4 : 3; }
      }
      if (event == Event::ArrowUp && *selected_ > 0) { (*selected_)--; return true; }
      if (event == Event::ArrowDown && *selected_ + 1 < count) { (*selected_)++; return true; }
      return false;
    }

  private:
    App& app_;
    int* selected_;
    Box box_;
  };

  renderer_ = Make<Impl>(app_, &selected_index_);
  return renderer_;
}
