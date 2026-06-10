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

void DidPage::DoRead() {
  if (did_input_.empty()) return;
  try {
    uint16_t did = std::stoul(did_input_, nullptr, 16);
    auto entry = DidDatabase::Instance().Find(did);
    std::stringstream ss;
    ss << "Reading DID 0x" << std::hex << std::uppercase << did << "\n";
    if (entry.did != 0) {
      ss << "Name: " << entry.name << "\n";
      ss << "Desc: " << entry.description << "\n";
      ss << "Size: " << (int)entry.data_size << " bytes";
    }
    result_text_ = ss.str();
    app_.ReadDid(did);
  } catch (...) {
    result_text_ = "Invalid DID format";
  }
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
    std::stringstream out;
    out << "Writing " << data.size() << " bytes to DID 0x"
        << std::hex << std::uppercase << did;
    result_text_ = out.str();
  } catch (...) {
    result_text_ = "Invalid input format";
  }
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

ftxui::Component DidPage::Build() {
  input_did_ = Input(&did_input_, " e.g. F190");
  input_data_ = Input(&data_input_, " hex data (e.g. 01 02)");

  btn_read_ = Button("Read DID", [this] { DoRead(); });
  btn_write_ = Button("Write DID", [this] { DoWrite(); });
  btn_search_ = Button("Search", [this] { DoSearch(); });

  auto container = Container::Vertical({
      input_did_,
      btn_search_,
      btn_read_,
      btn_write_,
      input_data_,
  });

  renderer_ = Renderer(container, std::function<Element()>([this] {
    Elements list_items;
    for (size_t i = 0; i < displayed_dids_.size(); i++) {
      const auto& e = displayed_dids_[i];
      std::stringstream ss;
      ss << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << e.did;
      std::string line = ss.str() + "  " + e.name;
      auto el = text(line);
      if ((int)i == selected_did_index_) el = el | bold | color(Color::Cyan);
      list_items.push_back(el);
    }
    if (list_items.empty()) {
      list_items.push_back(text(" No DID entries found "));
    }

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
               window(text(" DID List "), vbox(std::move(list_items)) | vscroll_indicator | yframe | flex),
               window(text(" Result "), paragraph(result_text_) | flex),
           }) |
           flex;
  }));

  return renderer_;
}
