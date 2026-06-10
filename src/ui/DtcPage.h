#pragma once

#include "app/App.h"
#include <ftxui/component/component.hpp>
#include <memory>
#include <string>
#include <vector>

class DtcPage {
public:
  explicit DtcPage(App& app);
  ftxui::Component Build();
  void Refresh();

private:
  App& app_;
  ftxui::Component renderer_;
  int selected_index_{0};
};
