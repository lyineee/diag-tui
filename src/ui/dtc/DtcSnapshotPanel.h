#pragma once

#include "app/App.h"
#include <ftxui/component/component.hpp>
#include <ftxui/screen/box.hpp>

class DtcSnapshotPanel {
public:
  explicit DtcSnapshotPanel(App& app);
  ftxui::Component Build();
  void Refresh();

private:
  App& app_;
  int selected_{0};
  ftxui::Box list_box_;
  ftxui::Component container_;
};
