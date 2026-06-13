#pragma once

#include "app/App.h"
#include "ui/dtc/DtcMaskFilter.h"
#include <ftxui/component/component.hpp>

class DtcListPanel {
public:
  explicit DtcListPanel(App& app);
  ftxui::Component Build();
  void Refresh();

private:
  App& app_;
  DtcMaskFilter mask_;
  int selected_{0};
};
