#include "ui/did/DidPage.h"
#include "uds/DidDatabase.h"
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>

#include <sstream>

using namespace ftxui;

DidPage::DidPage(App& app)
    : app_(app), browser_(app), polling_bar_(std::make_shared<DidPollingBar>(app)) {
  DidDatabase::Instance().Load("config/did_database.json");
  displayed_dids_ = DidDatabase::Instance().GetAll();
}

Component DidPage::Build() {
  menu_labels_ = {" Database ", " Browser "};
  auto menu_opt = MenuOption::HorizontalAnimated();
  menu_opt.entries = &menu_labels_;
  menu_opt.selected = &tab_selected_;
  auto menu = Menu(menu_opt);

  poll_flags_ = std::make_shared<std::vector<BoolFlag>>();
  poll_flags_->resize(displayed_dids_.size());
  did_items_.clear();

  Components did_components;
  for (size_t i = 0; i < displayed_dids_.size(); i++) {
    (*poll_flags_)[i].val = DidDatabase::Instance().GetExpanded(displayed_dids_[i].did);
    auto item = std::make_shared<DidItem>(app_, displayed_dids_[i], &(*poll_flags_)[i].val);
    did_components.push_back(item->Build());
    did_items_.push_back(item);
  }
  auto did_list = Container::Vertical(std::move(did_components), &did_selector_);

  app_.SetPollQuery([this] {
    std::vector<uint16_t> dids;
    for (size_t i = 0; i < displayed_dids_.size() && i < poll_flags_->size(); i++)
      if ((*poll_flags_)[i].val) dids.push_back(displayed_dids_[i].did);
    return dids;
  });

  auto browser_content = browser_.Build();
  auto polling_content = polling_bar_->Build(poll_flags_, displayed_dids_);
  auto tab_content = Container::Tab({did_list, browser_content}, &tab_selected_);

  renderer_ = Renderer(
    Container::Vertical({menu, tab_content, Container::Horizontal({polling_content})}),
    [this, menu, tab_content, polling_content] {
      Elements body;
      body.push_back(menu->Render());
      if (tab_selected_ == 0) {
        body.push_back(window(text(" DID List (Enter: expand, \u2191\u2193: navigate) "),
          tab_content->Render() | vscroll_indicator | yframe | flex) | flex);
        body.push_back(window(text(" Polling "), polling_content->Render()));
      } else {
        body.push_back(window(text(" DID Browser "), tab_content->Render() | flex) | flex);
      }
      return vbox(std::move(body)) | flex;
    });

  renderer_ |= CatchEvent([this](Event event) {
    if (!event.is_mouse()) return false;
    if (tab_selected_ != 0) return false;
    if (event.mouse().button == Mouse::WheelUp && did_selector_ > 0)
      { did_selector_--; return true; }
    if (event.mouse().button == Mouse::WheelDown &&
        did_selector_ + 1 < (int)displayed_dids_.size())
      { did_selector_++; return true; }
    return false;
  });

  return renderer_;
}
