#pragma once

#include "app/App.h"
#include "ui/DidItem.h"
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
  struct BoolHolder { bool val; };
  void TogglePolling(std::shared_ptr<std::vector<BoolHolder>> bools);

  App& app_;
  ftxui::Component renderer_;
  ftxui::Component input_did_;
  ftxui::Component input_data_;
  ftxui::Component btn_read_;
  ftxui::Component btn_write_;
  ftxui::Component btn_search_;
  ftxui::Component input_interval_;
  ftxui::Component btn_poll_;
  std::string did_input_;
  std::string data_input_;
  std::string interval_input_{"3"};
  uint16_t last_read_did_{0};
  std::vector<DidEntry> displayed_dids_;
  std::vector<std::shared_ptr<DidItem>> did_items_;  // keep DidItem alive
};
