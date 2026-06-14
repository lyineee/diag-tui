#pragma once

#include "app/App.h"
#include <ftxui/component/component.hpp>

class ConnectionPanel {
public:
  explicit ConnectionPanel(App& app);
  ftxui::Component Build();

private:
  void DoConnect();
  void DoDiscover();
  App& app_;
  std::string ip_;
  std::string src_;
  std::string tgt_;
};
