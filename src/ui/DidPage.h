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
  void DoRead(uint16_t did);
  void DoWrite();
  void DoSearch();
  std::string FormatResponse(uint16_t did, const std::vector<uint8_t>& data);

  App& app_;
  ftxui::Component renderer_;
  ftxui::Component input_did_;
  ftxui::Component input_data_;
  ftxui::Component btn_read_;
  ftxui::Component btn_write_;
  ftxui::Component btn_search_;
  ftxui::Component did_menu_;
  std::string did_input_;
  std::string data_input_;
  std::string result_text_;
  int selected_did_index_{0};
  uint16_t last_read_did_{0};
  std::vector<DidEntry> displayed_dids_;
  std::vector<std::string> did_menu_items_;
};
