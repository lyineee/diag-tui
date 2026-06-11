#include "ui/DidItem.h"
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>

#include <iomanip>
#include <sstream>

using namespace ftxui;

DidItem::DidItem(App& app, const DidEntry& entry, bool* expanded)
    : app_(app), entry_(entry), expanded_(expanded) {
  std::stringstream ss;
  ss << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << entry.did
     << "  " << entry.name;
  label_ = ss.str();
}

ftxui::Component DidItem::Build() {
  // Content shown when expanded: value display + graph
  auto content = Renderer(std::function<Element()>([this] {
    auto& state = app_.GetState();
    std::lock_guard<std::recursive_mutex> lock(state.mtx);

    Elements elems;
    auto val_it = state.did_values.find(entry_.did);

    auto val_entry = DidDatabase::Instance().Find(entry_.did);
    std::stringstream val_ss;
    if (val_entry.did != 0) {
      val_ss << val_entry.name << "  (0x" << std::hex << std::uppercase
             << std::setfill('0') << std::setw(4) << entry_.did << ")\n";
    }
    if (val_it != state.did_values.end()) {
      val_ss << "Size: " << std::dec << (int)val_it->second.raw.size() << " bytes\n";
      val_ss << "Hex:  " << val_it->second.display;
      if (val_it->second.is_numeric) {
        val_ss << "\nDec:  " << std::dec << val_it->second.numeric_value;
      }
    } else {
      val_ss << "No value read yet";
    }
    elems.push_back(paragraph(val_ss.str()) | color(Color::GreenLight));

    if (val_it != state.did_values.end() && val_it->second.is_numeric) {
      auto hist_it = state.did_history.find(entry_.did);
      if (hist_it != state.did_history.end() && hist_it->second.size() >= 2) {
        int min_v = hist_it->second[0], max_v = hist_it->second[0];
        for (auto v : hist_it->second) {
          if (v < min_v) min_v = v;
          if (v > max_v) max_v = v;
        }
        std::stringstream glbl;
        glbl << " Graph (min=" << min_v << " max=" << max_v
              << " " << hist_it->second.size() << " pts) ";

        auto graph_data = [this](int w, int h) -> std::vector<int> {
          auto& s = app_.GetState();
          std::lock_guard<std::recursive_mutex> lk(s.mtx);
          auto it = s.did_history.find(entry_.did);
          if (it == s.did_history.end() || it->second.empty()) {
            return std::vector<int>(w * 2, h / 2);
          }
          const auto& hist = it->second;
          int mn = hist[0], mx = hist[0];
          for (auto v : hist) { if (v < mn) mn = v; if (v > mx) mx = v; }
          if (mx == mn) mx = mn + 1;
          std::vector<int> result(w * 2);
          for (int i = 0; i < w * 2; i++) {
            size_t idx = (size_t)i * hist.size() / (w * 2);
            if (idx >= hist.size()) idx = hist.size() - 1;
            result[i] = (h - 1) - (hist[idx] - mn) * (h - 1) / (mx - mn);
          }
          return result;
        };

        elems.push_back(separator());
        elems.push_back(window(text(glbl.str()),
            graph(graph_data) | size(WIDTH, EQUAL, 48) | size(HEIGHT, EQUAL, 8)));
      }
    }
    return vbox(std::move(elems));
  }));

  return Collapsible(ConstStringRef(&label_), content, expanded_);
}
