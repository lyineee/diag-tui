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
  auto mask_component = mask_.Build();
  mask_component_ = mask_component;
  mask_.OnChange([this](uint8_t m) {
    auto& state = app_.GetState();
    state.dtc_status_mask = m;
  });

  auto btn_refresh = Button("Refresh (F5)", [this] { Refresh(); }, ButtonOption::Ascii());
  auto btn_clear = Button("Clear (F6)", [this] { app_.ClearDtc(); }, ButtonOption::Ascii());
  auto btn_bar = Container::Horizontal({btn_refresh, btn_clear});

  auto list_renderer = Renderer([this, btn_refresh, btn_clear](bool) -> Element {
    auto& state = app_.GetState();
    std::lock_guard<std::recursive_mutex> lock(state.mtx);
    const auto& dtcs = state.dtc_list;
    int count = (int)dtcs.size();
    if (selected_ >= count) selected_ = count > 0 ? count - 1 : 0;

    Elements dtc_els;
    for (int i = 0; i < count; i++) {
      const auto& dtc = dtcs[i];
      auto code = dtc.CodeStr();
      auto entry = DtcDatabase::Instance().Find(code);
      auto dot = text(i == selected_ ? " > " : "   ");
      auto line = text(code) | bold;
      Elements dots;
      auto mk = [&](uint8_t m, Color c) {
        dots.push_back(text(dtc.status & m ? "\u25CF" : "\u25CB") | color(c));
      };
      mk(0x01, Color::RedLight); mk(0x08, Color::YellowLight);
      mk(0x04, Color::Orange1); mk(0x20, Color::Cyan);
      auto name_el = text(" ") | size(WIDTH, EQUAL, 0);
      if (!entry.name.empty()) name_el = text(" " + entry.name) | dim;
      auto row = hbox({dot, line, separator(), hbox(std::move(dots)),
                       separator(), name_el | flex});
      if (i == selected_) row = row | inverted;
      dtc_els.push_back(row);
    }
    if (dtc_els.empty()) dtc_els.push_back(text(" No DTCs found "));

    auto list_el = window(
        text(" DTC List (" + std::to_string(count) + " DTCs) "),
        vbox(std::move(dtc_els)) | vscroll_indicator | yframe) | flex | reflect(list_box_);

    Element detail_el;
    if (selected_ >= 0 && selected_ < count) {
      const auto& dtc = dtcs[selected_];
      auto code = dtc.CodeStr();
      auto entry = DtcDatabase::Instance().Find(code);
      Elements rows;
      rows.push_back(hbox({text(" DTC ") | bold, text(" " + code + " ") | color(Color::Cyan) | bold}));
      if (!entry.name.empty())
        rows.push_back(hbox({text(" Name: ") | bold, text(entry.name) | dim}));
      if (!entry.description.empty())
        rows.push_back(hbox({text(" Desc: ") | bold,
          paragraph(" " + entry.description) | dim | size(WIDTH, EQUAL, 28)}));
      rows.push_back(separator());
      auto bit = [&](const char* l, uint8_t m, Color c) {
        auto v = text(dtc.status & m ? " Yes " : " No ");
        if (dtc.status & m) v = v | color(c) | bold;
        return hbox({text("  ") | dim, text(l) | bold, separator() | flex, v});
      };
      rows.push_back(bit("testFailed", 0x01, Color::RedLight));
      rows.push_back(bit("testFailedThisOpCycle", 0x02, Color::Red));
      rows.push_back(bit("pendingDTC", 0x04, Color::Orange1));
      rows.push_back(bit("confirmedDTC", 0x08, Color::YellowLight));
      rows.push_back(bit("testNotCompleteSinceClr", 0x10, Color::GrayDark));
      rows.push_back(bit("testFailedSinceClear", 0x20, Color::Cyan));
      rows.push_back(bit("testNotCompleteThisCycle", 0x40, Color::Yellow));
      rows.push_back(bit("warningIndicatorRequested", 0x80, Color::Red));
      rows.push_back(separator());
      rows.push_back(hbox({text(" Raw:  0x"), text(std::to_string(dtc.dtc_number)) | dim}));
      rows.push_back(hbox({text(" Stat: 0x"), text(std::to_string(dtc.status)) | dim}));
      rows.push_back(separator());
      rows.push_back(text(" Legend ") | bold);
      rows.push_back(hbox({text("  ") | dim, text("\u25CF") | color(Color::RedLight), text(" testFailed")}));
      rows.push_back(hbox({text("  ") | dim, text("\u25CF") | color(Color::YellowLight), text(" confirmedDTC")}));
      rows.push_back(hbox({text("  ") | dim, text("\u25CF") | color(Color::Orange1), text(" pendingDTC")}));
      rows.push_back(hbox({text("  ") | dim, text("\u25CF") | color(Color::Cyan), text(" testFailedSinceClear")}));
      detail_el = window(text(" DTC Detail "), vbox(std::move(rows))) | size(WIDTH, EQUAL, 38);
    } else {
      detail_el = window(text(" DTC Detail "), text(" Select a DTC from the list ")) | size(WIDTH, EQUAL, 38);
    }

    auto status = text(" " + std::to_string(count) + " DTCs  |  Count: "
      + std::to_string(state.dtc_count) + "  |  Mask: 0x" + MaskHex(state.dtc_status_mask)) | dim;

    Elements body;
    body.push_back(hbox({list_el, separator(), detail_el}) | flex);
    body.push_back(separator());
    body.push_back(hbox({btn_refresh->Render(), separatorEmpty(), btn_clear->Render(),
                         separator() | flex, status}));
    return vbox(std::move(body)) | flex;
  });

  list_renderer |= CatchEvent([this](Event event) {
    if (event.is_mouse()) {
      if (event.mouse().button == Mouse::WheelDown) {
        auto& state = app_.GetState();
        std::lock_guard<std::recursive_mutex> lock(state.mtx);
        if (selected_ + 1 < (int)state.dtc_list.size()) { selected_++; return true; }
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
            if (row < (int)state.dtc_list.size()) { selected_ = row; return true; }
          }
        }
      }
      return false;
    }
    if (event == Event::ArrowDown || event == Event::Character('j')) {
      auto& state = app_.GetState();
      std::lock_guard<std::recursive_mutex> lock(state.mtx);
      if (selected_ + 1 < (int)state.dtc_list.size()) { selected_++; return true; }
    }
    if (event == Event::ArrowUp || event == Event::Character('k')) {
      if (selected_ > 0) { selected_--; return true; }
    }
    if (event == Event::F5) { Refresh(); return true; }
    if (event == Event::F6) { app_.ClearDtc(); return true; }
    return false;
  });

  auto btn_bar_wrap = Renderer(btn_bar, [] { return text(""); });

  Components children;
  children.push_back(mask_component);
  children.push_back(list_renderer);
  children.push_back(btn_bar_wrap);
  container_ = Container::Vertical(std::move(children));
  container_ |= CatchEvent([this](Event event) {
    if (!mask_.expanded) return false;
    if (event == Event::ArrowDown || event == Event::ArrowUp)
      return mask_component_->OnEvent(event);
    return false;
  });

  RegisterGlobalKeys();

  return container_;
}

void DtcListPanel::RegisterGlobalKeys() {
  app_.RegisterKeyHandler([this](Event event) {
    if (app_.GetState().current_page != NavPage::Dtc) return false;

    if (mask_.expanded) {
      if (event == Event::Character('a')) {
        mask_.SetAll();
        auto& state = app_.GetState();
        state.dtc_status_mask = mask_.mask;
        return true;
      }
      if (event == Event::Character('r')) {
        mask_.Invert();
        auto& state = app_.GetState();
        state.dtc_status_mask = mask_.mask;
        return true;
      }
      if (event == Event::Character('m') || event == Event::Escape) {
        mask_.expanded = false;
        return true;
      }
      return false;
    }
    if (event == Event::Character('m')) {
      mask_.expanded = true;
      return true;
    }
    return false;
  });
}
