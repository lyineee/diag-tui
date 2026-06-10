#pragma once

#include "app/App.h"
#include <ftxui/component/component.hpp>
#include <memory>
#include <string>

class SessionManager {
public:
  explicit SessionManager(App& app);
  ftxui::Component Build();

private:
  App& app_;
  ftxui::Component btn_default_;
  ftxui::Component btn_extended_;
  ftxui::Component btn_programming_;
  ftxui::Component btn_tester_present_;
  ftxui::Component btn_reset_hard_;
  ftxui::Component btn_reset_soft_;
  ftxui::Component status_view_;
  std::string status_text_;
};
