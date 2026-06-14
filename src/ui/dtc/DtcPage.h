#pragma once

#include "app/App.h"
#include "ui/dtc/DtcListPanel.h"
#include "ui/dtc/DtcSnapshotPanel.h"
#include <ftxui/component/component.hpp>
#include <string>
#include <vector>

class DtcPage {
public:
  explicit DtcPage(App& app);
  ftxui::Component Build();
  void Refresh();
  bool NextTab();
  bool PrevTab();

private:
  App& app_;
  DtcListPanel list_panel_;
  DtcSnapshotPanel snapshot_panel_;
  ftxui::Component renderer_;
  std::vector<std::string> tabs_;
  int tab_selected_{0};
};
