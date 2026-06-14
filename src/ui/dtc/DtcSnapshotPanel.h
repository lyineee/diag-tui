#pragma once

#include "app/App.h"
#include <ftxui/component/component.hpp>

class DtcSnapshotPanel {
public:
  explicit DtcSnapshotPanel(App& app);
  ftxui::Component Build();
  void Refresh();

private:
  App& app_;
  int selected_{0};
};
