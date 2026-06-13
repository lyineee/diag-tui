#pragma once

#include <ftxui/dom/elements.hpp>
#include <ftxui/component/event.hpp>
#include <cstdint>

class DtcMaskFilter {
public:
  bool expanded{false};
  int cursor{0};
  uint8_t mask{0xFF};

  ftxui::Element Render();
  bool HandleEvent(ftxui::Event event, uint8_t& mask_out);
};
