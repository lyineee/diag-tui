#pragma once

#include "app/App.h"
#include "ui/DidItem.h"
#include "ui/did/DidBrowserPanel.h"
#include "ui/did/DidPollingBar.h"
#include "uds/UdsTypes.h"
#include <ftxui/component/component.hpp>
#include <memory>
#include <vector>

class DidPage {
public:
  explicit DidPage(App& app);
  ftxui::Component Build();

private:
  App& app_;
  ftxui::Component renderer_;
  DidBrowserPanel browser_;
  std::shared_ptr<DidPollingBar> polling_bar_;
  std::vector<DidEntry> displayed_dids_;
  std::vector<std::shared_ptr<DidItem>> did_items_;
  int did_selector_{0};
  int tab_selected_{0};
  std::vector<std::string> menu_labels_;
  std::shared_ptr<std::vector<BoolFlag>> poll_flags_;
};
