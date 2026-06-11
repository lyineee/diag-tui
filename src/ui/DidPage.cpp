#include "ui/DidPage.h"
#include "uds/DidDatabase.h"
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/mouse.hpp>
#include <spdlog/spdlog.h>

#include <iomanip>
#include <sstream>

using namespace ftxui;

DidPage::DidPage(App& app) : app_(app) {
  DidDatabase::Instance().Load("config/did_database.json");
  displayed_dids_ = DidDatabase::Instance().GetAll();
  interval_input_ = std::to_string(DidDatabase::Instance().GetPollInterval());
}

void DidPage::DoRead(uint16_t did) { last_read_did_ = did; }

void DidPage::DoWrite() {
  if (did_input_.empty()) return;
  try {
    uint16_t did = std::stoul(did_input_, nullptr, 16);
    std::vector<uint8_t> data;
    std::stringstream ss(data_input_);
    int byte;
    while (ss >> std::hex >> byte) data.push_back((uint8_t)byte);
    app_.WriteDid(did, data);
  } catch (...) {}
}

void DidPage::CollapseAll(std::shared_ptr<std::vector<BoolHolder>> bools) {
  for (auto& b : *bools) b.val = false;
  for (auto& e : displayed_dids_) DidDatabase::Instance().SetExpanded(e.did, false);
  DidDatabase::Instance().Save("config/did_database.json");
}

void DidPage::TogglePolling(std::shared_ptr<std::vector<BoolHolder>> bools) {
  bool do_stop = false;
  std::vector<uint16_t> poll_dids;
  int interval = 3;

  {
    auto& state = app_.GetState();
    std::lock_guard<std::recursive_mutex> lock(state.mtx);
    if (state.polling_active) {
      do_stop = true;
    } else {
      for (size_t i = 0; i < displayed_dids_.size() && i < bools->size(); i++) {
        if (bools->at(i).val) poll_dids.push_back(displayed_dids_[i].did);
      }
      try { interval = std::stoi(interval_input_); }
      catch (...) {}
      if (interval < 1) interval = 1;
    }
  }

  DidDatabase::Instance().SetPollInterval(interval);
  DidDatabase::Instance().Save("config/did_database.json");

  if (do_stop)
    app_.StopPolling();
  else if (!poll_dids.empty())
    app_.StartPolling(poll_dids, interval);
}

ftxui::Component DidPage::Build() {
  // ── Animated horizontal menu ───────────────────────────────────
  menu_labels_ = {" Database ", " Browser "};

  auto menu_opt = MenuOption::HorizontalAnimated();
  menu_opt.entries = &menu_labels_;
  menu_opt.selected = &tab_selected_;
  auto menu = Menu(menu_opt);

  // ── Controls (shared by both tabs) ────────────────────────────
  input_did_ = Input(&did_input_, " e.g. F190");
  input_data_ = Input(&data_input_, " hex data (e.g. 01 02)");
  input_interval_ = Input(&interval_input_, " sec");

  btn_read_ = Button("Read", [this] {
    if (!did_input_.empty()) {
      try {
        uint16_t did = (uint16_t)std::stoul(did_input_, nullptr, 16);
        DoRead(did);
        app_.ReadDidManual(did);
      } catch (...) {}
    }
  });

  btn_write_ = Button("Write", [this] { DoWrite(); });

  auto bools_ptr = std::make_shared<std::vector<BoolHolder>>();
  btn_poll_ = Button("Toggle", [this, bools_ptr] { TogglePolling(bools_ptr); });
  btn_collapse_all_ = Button(" Collapse All ", [this, bools_ptr] { CollapseAll(bools_ptr); });

  // ── DID list (Database tab) ──────────────────────────────────
  Components did_components;
  bools_ptr->resize(displayed_dids_.size());
  did_items_.clear();

  for (size_t i = 0; i < displayed_dids_.size(); i++) {
    (*bools_ptr)[i].val = DidDatabase::Instance().GetExpanded(displayed_dids_[i].did);
    auto item = std::make_shared<DidItem>(app_, displayed_dids_[i], &(*bools_ptr)[i].val);
    did_components.push_back(item->Build());
    did_items_.push_back(item);
  }
  auto did_list = Container::Vertical(std::move(did_components), &did_selector_);

  // Dynamic polling query
  app_.SetPollQuery([bools_ptr, this] {
    std::vector<uint16_t> dids;
    for (size_t i = 0; i < displayed_dids_.size() && i < bools_ptr->size(); i++)
      if ((*bools_ptr)[i].val) dids.push_back(displayed_dids_[i].did);
    return dids;
  });

  // ── Result renderer for Browser tab ──────────────────────────
  auto result_box = Renderer(std::function<Element()>([this] {
    auto& state = app_.GetState();
    std::lock_guard<std::recursive_mutex> lock(state.mtx);
    if (state.last_manual_did_read == 0 || state.last_manual_did_response.empty())
      return text("") | size(WIDTH, EQUAL, 0) | size(HEIGHT, EQUAL, 0);
    auto entry = DidDatabase::Instance().Find(state.last_manual_did_read);
    std::stringstream ss;
    ss << "  0x" << std::hex << std::uppercase << state.last_manual_did_read;
    if (entry.did != 0) ss << "  " << entry.name;
    ss << "\n  Hex: ";
    for (auto b : state.last_manual_did_response)
      ss << std::hex << std::setfill('0') << std::setw(2) << (int)b << " ";
    return paragraph(ss.str()) | color(Color::GreenLight);
  }));

  // ── Browser tab content (with labels) ────────────────────────
  auto browser_inner = Container::Vertical({
      Container::Horizontal({input_did_, btn_read_, btn_write_}),
      input_data_,
      result_box,
  });

  auto browser_content = Renderer(browser_inner, std::function<Element()>([this, browser_inner] {
    return vbox({
        hbox({text(" DID: ") | bold, browser_inner->ChildAt(0)->Render() | flex}),
        separator(),
        hbox({text(" Data: ") | bold, browser_inner->ChildAt(1)->Render() | flex}),
        separator(),
        hbox({text(" Result: ") | bold | color(Color::GreenLight),
              browser_inner->ChildAt(2)->Render() | flex}),
    });
  }));

  // ── Polling row (always visible) ────────────────────────────
  auto polling_render = Renderer(std::function<Element()>([this, bools_ptr] {
    auto& state = app_.GetState();
    std::lock_guard<std::recursive_mutex> lock(state.mtx);
    int expanded_count = 0;
    for (size_t i = 0; i < bools_ptr->size(); i++)
      if ((*bools_ptr)[i].val) expanded_count++;
    return hbox({
        text(" Interval: ") | bold,
        input_interval_->Render() | size(WIDTH, EQUAL, 4),
        text(" s "),
        btn_poll_->Render(),
        separator(),
        text(state.polling_active ? " Active " : " Idle ") |
            (state.polling_active ? color(Color::GreenLight) : color(Color::GrayDark)),
        separator(),
        text(std::to_string(expanded_count) + " DID(s) monitored ") | dim,
        separator(),
        btn_collapse_all_->Render(),
    });
  }));
  auto polling_row = Container::Horizontal({input_interval_, btn_poll_, btn_collapse_all_});

  // ── Tab container ──────────────────────────────────────────────
  auto tab_content = Container::Tab({did_list, browser_content}, &tab_selected_);

  // ── Outer renderer ─────────────────────────────────────────────
  renderer_ = Renderer(
      Container::Vertical({menu, tab_content, polling_row}),
      std::function<Element()>([this, menu, tab_content, polling_render] {
        Elements body;
        body.push_back(menu->Render());

        if (tab_selected_ == 0) {
          // Database tab: DID list + polling
          body.push_back(
              window(text(" DID List (Enter: expand, \u2191\u2193: navigate) "),
                     tab_content->Render() | vscroll_indicator | yframe | flex) |
              flex);
          body.push_back(window(text(" Polling "), polling_render->Render()));
        } else {
          // Browser tab: no polling, no window title
          body.push_back(
              window(text(" DID Browser "),
                     tab_content->Render() | vscroll_indicator | yframe | flex) |
              flex);
        }

        return vbox(std::move(body)) | flex;
      }));

  // Mouse wheel for DID list
  renderer_ |= CatchEvent([this](Event event) {
    if (!event.is_mouse()) return false;
    if (tab_selected_ != 0) return false;
    if (event.mouse().button == Mouse::WheelUp && did_selector_ > 0) {
      did_selector_--;
      return true;
    }
    if (event.mouse().button == Mouse::WheelDown &&
        did_selector_ + 1 < (int)displayed_dids_.size()) {
      did_selector_++;
      return true;
    }
    return false;
  });

  return renderer_;
}
