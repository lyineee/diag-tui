#pragma once

#include "app/App.h"
#include <ftxui/component/component.hpp>
#include <memory>
#include <string>

class RawPage {
public:
  explicit RawPage(App& app);
  ftxui::Component Build();

private:
  App& app_;
  ftxui::Component renderer_;
  ftxui::Component input_hex_;
  ftxui::Component btn_send_;
  std::string hex_input_;
};
