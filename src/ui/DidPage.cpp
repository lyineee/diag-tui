#include "ui/DidPage.h"
#include "uds/DidDatabase.h"
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/mouse.hpp>
#include <spdlog/spdlog.h>

#include <iomanip>
#include <sstream>

using namespace ftxui;

DidPage::DidPage(App& app) : app_(app) {
  DidDatabase::Instance().Load("config/did_database.json");
  displayed_dids_ = DidDatabase::Instance().GetAll();
  // Read polling interval from JSON
  interval_input_ = std::to_string(DidDatabase::Instance().GetPollInterval());
}

void DidPage::DoRead(uint16_t did) {
  last_read_did_ = did;
}

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
  // Persist all to JSON
  for (auto& e : displayed_dids_)
    DidDatabase::Instance().SetExpanded(e.did, false);
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
        if (bools->at(i).val)
          poll_dids.push_back(displayed_dids_[i].did);
      }
      try { interval = std::stoi(interval_input_); }
      catch (...) {}
      if (interval < 1) interval = 1;
    }
  }

  // Save polling interval to JSON
  DidDatabase::Instance().SetPollInterval(interval);
  DidDatabase::Instance().Save("config/did_database.json");

  if (do_stop)
    app_.StopPolling();
  else if (!poll_dids.empty())
    app_.StartPolling(poll_dids, interval);
}

ftxui::Component DidPage::Build() {
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

  // Build DID list, loading expanded state from JSON
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

  // Collapse All button
  btn_collapse_all_ = Button(" Collapse All ", [this, bools_ptr] {
    CollapseAll(bools_ptr);
  });

  renderer_ = Renderer(
      Container::Vertical({
          Container::Horizontal({input_did_, btn_read_, btn_write_}),
          input_data_,
          Container::Horizontal({input_interval_, btn_poll_, btn_collapse_all_}),
          did_list,
      }),
      std::function<Element()>([this, bools_ptr, did_list] {
        auto& state = app_.GetState();
        std::lock_guard<std::recursive_mutex> lock(state.mtx);

        // Result for manual reads
        std::string result;
        if (state.last_manual_did_read != 0 && !state.last_manual_did_response.empty()) {
          auto entry = DidDatabase::Instance().Find(state.last_manual_did_read);
          std::stringstream ss;
          ss << "  0x" << std::hex << std::uppercase << state.last_manual_did_read;
          if (entry.did != 0) ss << "  " << entry.name;
          ss << "\n  Hex: ";
          for (auto b : state.last_manual_did_response)
            ss << std::hex << std::setfill('0') << std::setw(2) << (int)b << " ";
          result = ss.str();
        }

        int expanded_count = 0;
        for (size_t i = 0; i < bools_ptr->size(); i++)
          if ((*bools_ptr)[i].val) expanded_count++;

        return vbox({
                   window(text(" DID Browser "),
                          vbox({
                              hbox({text(" DID: ") | bold, input_did_->Render() | flex,
                                    btn_read_->Render(), btn_write_->Render()}),
                              separator(),
                              hbox({text(" Data: ") | bold, input_data_->Render() | flex}),
                              separator(),
                              hbox({text(" Result: ") | bold,
                                    paragraph(result) | flex | color(Color::GreenLight)}),
                          })),
                   separator(),
                   window(text(" Polling "),
                          hbox({
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
                          })),
                   separator(),
                   window(text(" DID List (Enter: expand, \u2191\u2193: navigate) "),
                          did_list->Render() | vscroll_indicator | yframe | flex) | flex,
               }) |
               flex;
      }));

  // Mouse wheel support
  renderer_ |= CatchEvent([this](Event event) {
    if (!event.is_mouse()) return false;
    auto& state = app_.GetState();
    std::lock_guard<std::recursive_mutex> lock(state.mtx);
    (void)state;
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
