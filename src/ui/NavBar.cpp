#include "ui/NavBar.h"
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <spdlog/spdlog.h>

using namespace ftxui;

NavBar::NavBar(App& app) : app_(app) {
  items_ = {
      " DTC ",
      " DID ",
      " Raw Send ",
      " Session ",
      " Settings ",
  };

  MenuOption option;
  option.entries = &items_;
  option.selected = &selected_;
  option.on_change = [this] {
    if (page_cb_) page_cb_((NavPage)selected_);
  };

  menu_ = Menu(option);
}

ftxui::Component NavBar::Build() {
  auto renderer = Renderer(menu_, std::function<Element()>([this] {
    auto menu_element = menu_->Render();
    return window(text(" Navigation "),
                  vbox(menu_element) | size(WIDTH, EQUAL, 20));
  }));

  return renderer;
}

void NavBar::SetOnPageChange(std::function<void(NavPage)> cb) {
  page_cb_ = std::move(cb);
}
