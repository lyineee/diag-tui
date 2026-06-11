#include "ui/RawPage.h"
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <spdlog/spdlog.h>

#include <iomanip>
#include <sstream>

using namespace ftxui;

RawPage::RawPage(App& app) : app_(app) {}

ftxui::Component RawPage::Build() {
  input_hex_ = Input(&hex_input_, " hex bytes e.g. 22 F1 90");

  btn_send_ = Button("Send", [this] {
    if (hex_input_.empty()) return;
    std::vector<uint8_t> data;
    std::stringstream ss(hex_input_);
    int byte;
    while (ss >> std::hex >> byte) {
      data.push_back((uint8_t)byte);
    }
    app_.SendRaw(data);
  });

  auto container = Container::Vertical({
      input_hex_,
      btn_send_,
  });

  renderer_ = Renderer(container, std::function<Element()>([this] {
    auto& state = app_.GetState();
    std::lock_guard<std::recursive_mutex> lock(state.mtx);

    std::stringstream ss;
    ss << "Last Request:\n";
    for (auto b : state.last_raw_request) {
      ss << std::hex << std::setfill('0') << std::setw(2) << (int)b << " ";
    }
    ss << "\n\nLast Response:\n";
    ss << state.raw_response_text;

    return vbox({
               window(text(" Raw Send "),
                      vbox({
                          hbox({text(" Hex: ") | bold, input_hex_->Render() | flex}),
                          separator(),
                          hbox({btn_send_->Render()}),
                      })) | size(WIDTH, EQUAL, 80),
               window(text(" Response "), paragraph(ss.str()) | flex),
           }) |
           flex;
  }));

  return renderer_;
}
