#include "ui/dtc/DtcMaskFilter.h"
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>

#include <sstream>
#include <iomanip>

using namespace ftxui;

static std::string MaskHex(uint8_t m) {
  std::stringstream ss;
  ss << std::hex << (int)m;
  return ss.str();
}

void DtcMaskFilter::OnChange(std::function<void(uint8_t)> cb) { on_change_ = std::move(cb); }

void DtcMaskFilter::SetAll() {
  mask = 0xFF;
  for (int i = 0; i < 8; i++) bits_[i] = true;
  if (on_change_) on_change_(mask);
}

void DtcMaskFilter::Invert() {
  mask ^= 0xFF;
  for (int i = 0; i < 8; i++) bits_[i] = mask & (1 << i);
  if (on_change_) on_change_(mask);
}

void DtcMaskFilter::FocusFirst() {
  if (check_list_) check_list_->SetActiveChild(0);
}

Component DtcMaskFilter::Build() {
  static const char* kNames[] = {
    "testFailed", "testFailedThisOperationCycle",
    "pendingDTC", "confirmedDTC",
    "testNotCompletedSinceLastClear", "testFailedSinceLastClear",
    "testNotCompletedThisOperationCycle", "warningIndicatorRequested"
  };

  for (int i = 0; i < 8; i++) bits_[i] = mask & (1 << i);

  auto build = [this] {
    mask = 0;
    for (int i = 0; i < 8; i++) if (bits_[i]) mask |= (1 << i);
    if (on_change_) on_change_(mask);
  };

  auto reset_bits = [this] {
    for (int i = 0; i < 8; i++) bits_[i] = mask & (1 << i);
  };

  Components checks;
  for (int i = 0; i < 8; i++) checks.push_back(Checkbox(kNames[i], &bits_[i]));
  check_list_ = Container::Vertical(std::move(checks));

  auto btn_all = Button("All (a)", [this, build] {
    for (int i = 0; i < 8; i++) bits_[i] = true;
    build();
  }, ButtonOption::Ascii());

  auto btn_inv = Button("Invert (r)", [this, build] {
    for (int i = 0; i < 8; i++) bits_[i] = !bits_[i];
    build();
  }, ButtonOption::Ascii());

  auto btn_close = Button("Close (m)", [this, reset_bits] {
    reset_bits();
    expanded = false;
  }, ButtonOption::Ascii());

  auto btn_expand = Button("Configure (m)", [this, reset_bits] {
    reset_bits();
    expanded = true;
  }, ButtonOption::Ascii());

  auto btn_bar = Container::Horizontal({btn_all, btn_inv, btn_close});

  auto show_expanded = [this] { return expanded; };
  auto show_collapsed = [this] { return !expanded; };
  auto check_list_vis = Maybe(check_list_, show_expanded);
  auto btn_bar_vis = Maybe(btn_bar, show_expanded);
  auto btn_expand_vis = Maybe(btn_expand, show_collapsed);

  return Renderer(Container::Vertical({check_list_vis, btn_bar_vis, btn_expand_vis}), [=] {
    if (!expanded) {
      return hbox({
        text(" Mask: 0x" + MaskHex(mask)) | dim,
        text("  "),
        btn_expand->Render(),
      });
    }
    uint8_t old = mask;
    mask = 0;
    for (int i = 0; i < 8; i++) if (bits_[i]) mask |= (1 << i);
    if (mask != old && on_change_) on_change_(mask);
    return window(text(" DTC Status Mask "),
      vbox({check_list_->Render(), separator(), btn_bar->Render()}));
  }) | CatchEvent([this, build, reset_bits](Event event) {
    if (!expanded) {
      if (event == Event::Character('m')) { expanded = true; return true; }
      return false;
    }
    if (event == Event::Character('a')) {
      for (int i = 0; i < 8; i++) bits_[i] = true;
      build();
      return true;
    }
    if (event == Event::Character('r')) {
      for (int i = 0; i < 8; i++) bits_[i] = !bits_[i];
      build();
      return true;
    }
    if (event == Event::Character('m') || event == Event::Escape) {
      reset_bits();
      expanded = false;
      return true;
    }
    return false;
  });
}
