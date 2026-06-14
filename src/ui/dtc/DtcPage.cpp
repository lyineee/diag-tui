#include "ui/dtc/DtcPage.h"
#include "uds/DtcDatabase.h"
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>

using namespace ftxui;

DtcPage::DtcPage(App& app) : app_(app), list_panel_(app), snapshot_panel_(app) {
  DtcDatabase::Instance().Load("config/dtc_database.json");
}

void DtcPage::Refresh() {
  if (tab_selected_ == 0) list_panel_.Refresh();
  else snapshot_panel_.Refresh();
}

Component DtcPage::Build() {
  tabs_ = {" DTC List ", " Snapshots "};
  auto menu = Menu(&tabs_, &tab_selected_, MenuOption::HorizontalAnimated());
  auto tab_content = Container::Tab({list_panel_.Build(), snapshot_panel_.Build()}, &tab_selected_);
  auto content_flex = Renderer(tab_content, [tab_content] {
    return tab_content->Render() | flex;
  });
  renderer_ = Container::Vertical({menu, content_flex})
    | Renderer([](Element e) { return e | flex; });
  return renderer_;
}
