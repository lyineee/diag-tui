#include "ui/dtc/DtcPage.h"
#include "uds/DtcDatabase.h"
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>

using namespace ftxui;

DtcPage::DtcPage(App& app) : app_(app), list_panel_(app) {
  DtcDatabase::Instance().Load("config/dtc_database.json");
}

void DtcPage::Refresh() { list_panel_.Refresh(); }

Component DtcPage::Build() {
  tabs_ = {" DTC List "};
  auto menu = Menu(&tabs_, &tab_selected_, MenuOption::HorizontalAnimated());
  auto tab_content = Container::Tab({list_panel_.Build()}, &tab_selected_);

  renderer_ = Renderer(
    Container::Horizontal({menu, tab_content}),
    [this, menu, tab_content] {
      return vbox({
        menu->Render(),
        separator(),
        tab_content->Render() | flex,
      }) | flex;
    });

  return renderer_;
}
