#pragma once

#include <ftxui/component/component.hpp>
#include <functional>
#include <cstdint>

class DtcMaskFilter {
public:
  bool expanded{false};
  uint8_t mask{0xFF};

  ftxui::Component Build();
  void OnChange(std::function<void(uint8_t)> cb);
  void SetAll();
  void Invert();
  void FocusFirst();

private:
  std::function<void(uint8_t)> on_change_;
  bool bits_[8]{};
  ftxui::Component check_list_;
};
