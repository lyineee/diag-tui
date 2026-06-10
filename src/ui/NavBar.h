#pragma once

#include "app/App.h"
#include <ftxui/component/component.hpp>
#include <functional>
#include <memory>
#include <string>
#include <vector>

class NavBar {
public:
  explicit NavBar(App& app);
  ftxui::Component Build();
  void SetOnPageChange(std::function<void(NavPage)> cb);

private:
  App& app_;
  ftxui::Component menu_;
  std::function<void(NavPage)> page_cb_;
  std::vector<std::string> items_;
  int selected_{0};

  void OnMenuEntry(int index);
};
