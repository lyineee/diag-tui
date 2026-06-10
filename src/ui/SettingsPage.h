#pragma once

#include "app/App.h"
#include <ftxui/component/component.hpp>
#include <memory>
#include <string>
#include <vector>

class SettingsPage {
public:
  explicit SettingsPage(App& app);
  ftxui::Component Build();

private:
  App& app_;
  ftxui::Component renderer_;
  ftxui::Component input_ip_;
  ftxui::Component input_src_;
  ftxui::Component input_tgt_;
  ftxui::Component btn_connect_;
  ftxui::Component btn_disconnect_;
  ftxui::Component btn_discover_;
  std::string ip_input_;
  std::string src_input_;
  std::string tgt_input_;
  int selected_ecu_{0};

  void DoConnect();
  void DoDiscover();
  void SelectEcu(int index);
};
