#pragma once

#include "app/App.h"
#include "uds/UdsTypes.h"
#include <ftxui/component/component.hpp>
#include <memory>
#include <string>
#include <vector>

class DidPage {
public:
  explicit DidPage(App& app);
  ftxui::Component Build();

private:
  App& app_;
  ftxui::Component renderer_;
  ftxui::Component input_did_;
  ftxui::Component input_data_;
  ftxui::Component btn_read_;
  ftxui::Component btn_write_;
  ftxui::Component btn_search_;
  std::string did_input_;
  std::string data_input_;
  std::string result_text_;
  int selected_did_index_{0};
  std::vector<DidEntry> displayed_dids_;

  void DoRead();
  void DoWrite();
  void DoSearch();
};
