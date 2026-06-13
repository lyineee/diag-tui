#pragma once

#include "app/App.h"
#include "ui/dtc/DtcMaskFilter.h"
#include <ftxui/component/component.hpp>
#include <ftxui/screen/box.hpp>

class DtcListPanel {
public:
  explicit DtcListPanel(App& app);
  ftxui::Component Build();
  void Refresh();

private:
  void RegisterGlobalKeys();
  App& app_;
  DtcMaskFilter mask_;
  int selected_{0};
  ftxui::Box list_box_;
  ftxui::Component container_;
  ftxui::Component mask_component_;
};
