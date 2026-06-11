#include "ui/DidPage.h"
#include "uds/DidDatabase.h"
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>
#include <spdlog/spdlog.h>

#include <iomanip>
#include <sstream>

using namespace ftxui;

DidPage::DidPage(App& app) : app_(app) {
  DidDatabase::Instance().Load("config/did_database.json");
  displayed_dids_ = DidDatabase::Instance().GetAll();
}

void DidPage::DoRead(uint16_t did) {
  last_read_did_ = did;
  app_.ReadDid(did);
}

void DidPage::DoWrite() {
  if (did_input_.empty()) return;
  try {
    uint16_t did = std::stoul(did_input_, nullptr, 16);
    std::vector<uint8_t> data;
    std::stringstream ss(data_input_);
    int byte;
    while (ss >> std::hex >> byte) {
      data.push_back((uint8_t)byte);
    }
    app_.WriteDid(did, data);
  } catch (...) {}
}

void DidPage::DoSearch() {
  if (did_input_.empty()) {
    displayed_dids_ = DidDatabase::Instance().GetAll();
  } else {
    displayed_dids_ = DidDatabase::Instance().Search(did_input_);
    try {
      uint16_t hex_val = (uint16_t)std::stoul(did_input_, nullptr, 16);
      auto hex_did = DidDatabase::Instance().Find(hex_val);
      if (hex_did.did != 0) {
        bool found = false;
        for (auto& d : displayed_dids_) {
          if (d.did == hex_did.did) { found = true; break; }
        }
        if (!found) displayed_dids_.insert(displayed_dids_.begin(), hex_did);
      }
    } catch (...) {}
  }
}

void DidPage::TogglePolling(std::shared_ptr<std::vector<BoolHolder>> bools) {
  // Must release state.mtx BEFORE calling StopPolling/StartPolling
  // to avoid deadlock with the polling thread (which also locks state.mtx).
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
        if (bools->at(i).val) {
          poll_dids.push_back(displayed_dids_[i].did);
        }
      }
      try { interval = std::stoi(interval_input_); }
      catch (...) {}
      if (interval < 1) interval = 1;
    }
  }  // mutex released here

  if (do_stop) {
    app_.StopPolling();
  } else if (!poll_dids.empty()) {
    app_.StartPolling(poll_dids, interval);
  }
}

ftxui::Component DidPage::Build() {
  input_did_ = Input(&did_input_, " e.g. F190");
  input_data_ = Input(&data_input_, " hex data (e.g. 01 02)");
  input_interval_ = Input(&interval_input_, " sec");

  btn_read_ = Button("Read", [this] {
    if (!did_input_.empty()) {
      try { DoRead((uint16_t)std::stoul(did_input_, nullptr, 16)); }
      catch (...) {}
    }
  });

  btn_write_ = Button("Write", [this] { DoWrite(); });
  btn_search_ = Button("Search", [this] { DoSearch(); });

  auto bools_ptr = std::make_shared<std::vector<BoolHolder>>();
  btn_poll_ = Button("Toggle", [this, bools_ptr] { TogglePolling(bools_ptr); });

  // Build DID list using DidItem
  Components did_components;
  bools_ptr->resize(displayed_dids_.size());
  did_items_.clear();

  for (size_t i = 0; i < displayed_dids_.size(); i++) {
    bool* exp_ptr = &(*bools_ptr)[i].val;
    auto item = std::make_shared<DidItem>(app_, displayed_dids_[i], exp_ptr);
    did_components.push_back(item->Build());
    did_items_.push_back(item);  // keep alive (Renderer captures this)
  }

  auto did_list = Container::Vertical(std::move(did_components));

  auto controls = Container::Vertical({
      input_did_,
      btn_search_,
      btn_read_,
      btn_write_,
      input_data_,
  });

  auto poll_row = Container::Horizontal({
      input_interval_,
      btn_poll_,
  });

  renderer_ = Renderer(Container::Vertical({controls, poll_row, did_list}),
      std::function<Element()>([this, bools_ptr, did_list] {
    auto& state = app_.GetState();
    std::lock_guard<std::recursive_mutex> lock(state.mtx);

    std::string result;
    if (state.last_did_read != 0 && !state.last_did_response.empty()) {
      auto entry = DidDatabase::Instance().Find(state.last_did_read);
      auto val_it = state.did_values.find(state.last_did_read);
      std::stringstream ss;
      ss << "DID:  0x" << std::hex << std::uppercase << state.last_did_read << "\n";
      if (entry.did != 0) {
        ss << "Name: " << entry.name << "\n";
        ss << "Desc: " << entry.description << "\n";
      }
      if (val_it != state.did_values.end()) {
        ss << "Size: " << std::dec << (int)val_it->second.raw.size() << " bytes\n";
        ss << "Hex:  " << val_it->second.display;
        if (val_it->second.is_numeric) {
          ss << "\nDec:  " << std::dec << val_it->second.numeric_value;
        }
      }
      result = ss.str();
    }

    int expanded_count = 0;
    for (size_t i = 0; i < bools_ptr->size(); i++) if ((*bools_ptr)[i].val) expanded_count++;

    return vbox({
               window(text(" DID Browser "),
                      vbox({
                          hbox({text(" DID: ") | bold, input_did_->Render() | flex, btn_search_->Render()}),
                          separator(),
                          hbox({btn_read_->Render(), btn_write_->Render()}),
                          separator(),
                          window(text(" Write Data "),
                                 hbox({text(" Data: ") | bold, input_data_->Render() | flex})),
                      })) | size(WIDTH, EQUAL, 60),
               window(text(" DID List (Enter: expand/collapse, arrows: navigate) "),
                      did_list->Render() | vscroll_indicator | yframe | flex),
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
                      })),
               window(text(" Result "), paragraph(result) | flex),
           }) |
           flex;
  }));

  return renderer_;
}
