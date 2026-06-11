#include "ui/DidItem.h"
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/screen/box.hpp>

#include <iomanip>
#include <sstream>

using namespace ftxui;

DidItem::DidItem(App& app, const DidEntry& entry, bool* expanded)
    : app_(app), entry_(entry), expanded_(expanded) {
  std::stringstream ss;
  ss << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << entry.did
     << "  " << entry.name;
  label_ = ss.str();
  graph_checkbox_ = entry.graphable;
}

ftxui::Component DidItem::Build() {
  // ── toggle button (same as Collapsible internal) ──────────────
  CheckboxOption toggle_opt;
  toggle_opt.transform = [](EntryState s) {
    auto prefix = text(s.state ? "▼ " : "▶ ");
    auto t = text(s.label);
    if (s.active) t |= bold;
    if (s.focused) t |= inverted;
    return hbox({prefix, t});
  };
  auto toggle = Checkbox(label_, expanded_, toggle_opt);

  // ── value display (Renderer, no event handling needed) ────────
  auto value_renderer = Renderer(std::function<Element()>([this] {
    auto& state = app_.GetState();
    std::lock_guard<std::recursive_mutex> lock(state.mtx);
    auto val_it = state.did_values.find(entry_.did);
    auto ve = DidDatabase::Instance().Find(entry_.did);
    std::stringstream ss;
    if (ve.did != 0) {
      ss << ve.name << "  (0x" << std::hex << std::uppercase
         << std::setfill('0') << std::setw(4) << entry_.did << ")\n";
    }
    if (val_it != state.did_values.end()) {
      ss << "Size: " << std::dec << (int)val_it->second.raw.size() << " bytes\n";
      ss << "Hex:  " << val_it->second.display;
      if (val_it->second.is_numeric)
        ss << "\nDec:  " << std::dec << val_it->second.numeric_value;
    } else {
      ss << "No value read yet";
    }
    return paragraph(ss.str()) | color(Color::GreenLight);
  }));

  // ── refresh button ──────────────────────────────────────────
  ButtonOption btn_opt;
  btn_opt.transform = [](EntryState s) {
    auto el = text(s.label);
    if (s.focused) el = el | bold | color(Color::Cyan);
    return el;
  };
  auto refresh_btn = Button(" [Refresh] ", [this] { app_.ReadDid(entry_.did); }, btn_opt);

  // ── graph checkbox ──────────────────────────────────────────
  Component graph_chk;
  if (entry_.graphable) {
    graph_chk = Checkbox(" Graph ", &graph_checkbox_);
  }

  // ── graph display (Maybe, shown only when graph is enabled) ──
  auto graph_renderer = Renderer(std::function<Element()>([this] {
    auto& state = app_.GetState();
    std::lock_guard<std::recursive_mutex> lock(state.mtx);
    auto val_it = state.did_values.find(entry_.did);
    if (!graph_checkbox_ || val_it == state.did_values.end() || !val_it->second.is_numeric)
      return text("") | size(WIDTH, EQUAL, 0) | size(HEIGHT, EQUAL, 0);

    auto hist_it = state.did_history.find(entry_.did);
    if (hist_it == state.did_history.end() || hist_it->second.size() < 2)
      return text("") | size(WIDTH, EQUAL, 0) | size(HEIGHT, EQUAL, 0);

    int mn = hist_it->second[0], mx = hist_it->second[0];
    for (auto v : hist_it->second) { if (v < mn) mn = v; if (v > mx) mx = v; }
    std::stringstream lbl;
    lbl << " Graph (min=" << mn << " max=" << mx << " " << hist_it->second.size() << " pts) ";

    auto graph_fn = [this](int w, int h) -> std::vector<int> {
      auto& s = app_.GetState();
      std::lock_guard<std::recursive_mutex> lk(s.mtx);
      auto it = s.did_history.find(entry_.did);
      if (it == s.did_history.end() || it->second.empty())
        return std::vector<int>(w * 2, h / 2);
      const auto& hist = it->second;
      int mn = hist[0], mx = hist[0];
      for (auto v : hist) { if (v < mn) mn = v; if (v > mx) mx = v; }
      if (mx == mn) mx = mn + 1;
      std::vector<int> r(w * 2);
      for (int i = 0; i < w * 2; i++) {
        size_t idx = (size_t)i * hist.size() / (w * 2);
        if (idx >= hist.size()) idx = hist.size() - 1;
        r[i] = (h - 1) - (hist[idx] - mn) * (h - 1) / (mx - mn);
      }
      return r;
    };

    return vbox({
        separator(),
        window(text(lbl.str()), graph(graph_fn) | size(WIDTH, EQUAL, 48) | size(HEIGHT, EQUAL, 8)),
    });
  }));
  auto graph_maybe = Maybe(graph_renderer, &graph_checkbox_);

  // ── content layout (visible only when expanded) ──────────────
  Component content_inner;
  if (graph_chk) {
    auto controls = Container::Horizontal({refresh_btn, graph_chk});
    content_inner = Container::Vertical({value_renderer, controls, graph_maybe});
  } else {
    content_inner = Container::Vertical({value_renderer, refresh_btn});
  }
  auto content = Maybe(content_inner, expanded_);

  return Container::Vertical({toggle, content});
}
