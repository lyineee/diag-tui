#include "ui/did/DidBrowserPanel.h"
#include "uds/DidDatabase.h"
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>

#include <sstream>
#include <iomanip>

using namespace ftxui;

DidBrowserPanel::DidBrowserPanel(App& app) : app_(app) {}

void DidBrowserPanel::DoWrite() {
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

Component DidBrowserPanel::Build() {
  auto input_did = Input(&did_input_, " e.g. F190");
  auto input_data = Input(&data_input_, " hex data (e.g. 01 02)");
  auto btn_read = Button("Read", [this] {
    if (!did_input_.empty()) {
      try {
        uint16_t did = (uint16_t)std::stoul(did_input_, nullptr, 16);
        app_.ReadDidManual(did);
      } catch (...) {}
    }
  });
  auto btn_write = Button("Write", [this] { DoWrite(); });

  auto result_box = Renderer([this] {
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
  });

  auto inner = Container::Vertical({Container::Horizontal({input_did, btn_read, btn_write}), input_data, result_box});

  return Renderer(inner, [inner] {
    return vbox({
      hbox({text(" DID: ") | bold, inner->ChildAt(0)->Render() | flex}),
      separator(),
      hbox({text(" Data: ") | bold, inner->ChildAt(1)->Render() | flex}),
      separator(),
      hbox({text(" Result: ") | bold | color(Color::GreenLight), inner->ChildAt(2)->Render() | flex}),
    });
  });
}
