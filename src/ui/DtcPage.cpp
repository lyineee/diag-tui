#include "ui/DtcPage.h"
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>
#include <spdlog/spdlog.h>

#include <sstream>

using namespace ftxui;

DtcPage::DtcPage(App& app) : app_(app) {
  Refresh();
}

void DtcPage::Refresh() {
  app_.ReadDtc();
}

ftxui::Component DtcPage::Build() {
  renderer_ = Renderer([this] {
    auto& state = app_.GetState();
    std::lock_guard<std::mutex> lock(state.mtx);

    if (selected_index_ >= (int)state.dtc_list.size()) {
      selected_index_ = state.dtc_list.empty() ? 0 : (int)state.dtc_list.size() - 1;
    }

    Elements dtc_elements;
    for (size_t i = 0; i < state.dtc_list.size(); i++) {
      const auto& dtc = state.dtc_list[i];
      uint32_t code = dtc.dtc_number;
      uint8_t first_byte = (code >> 16) & 0xFF;
      uint16_t rest = code & 0xFFFF;

      std::string code_str;
      switch (first_byte >> 4) {
        case 0: code_str = "P"; break;
        case 1: code_str = "C"; break;
        case 2: code_str = "B"; break;
        case 3: code_str = "U"; break;
        default: code_str = "X"; break;
      }
      char buf[8];
      snprintf(buf, sizeof(buf), "%04X", rest);
      code_str += buf;

      std::string status_str;
      if (dtc.status & 0x01) status_str += "testFailed ";
      if (dtc.status & 0x02) status_str += "testThisCycle ";
      if (dtc.status & 0x04) status_str += "pending ";
      if (dtc.status & 0x08) status_str += "confirmed ";
      if (dtc.status & 0x10) status_str += "testNotSinceClear ";
      if (dtc.status & 0x20) status_str += "testFailedThisCycle ";
      if (dtc.status & 0x40) status_str += "warning ";
      if (status_str.empty()) status_str = "none";

      std::string line = code_str + "  [" + status_str + "]";
      auto el = text(line);
      if ((int)i == selected_index_) el = el | bold | color(Color::Cyan) | bgcolor(Color::GrayDark);
      dtc_elements.push_back(el);
    }

    if (dtc_elements.empty()) {
      dtc_elements.push_back(text(" No DTCs found "));
    }

    auto dtc_list_element = window(text(" DTC List (F5: Refresh, F6: Clear, \u2191\u2193: Select) "),
                                    vbox(std::move(dtc_elements)) | vscroll_indicator | yframe) |
                            size(WIDTH, EQUAL, 50) | flex;

    std::string detail_text;
    if (selected_index_ >= 0 &&
        (size_t)selected_index_ < state.dtc_list.size()) {
      const auto& dtc = state.dtc_list[selected_index_];
      uint32_t code = dtc.dtc_number;
      uint8_t first_byte = (code >> 16) & 0xFF;
      uint16_t rest = code & 0xFFFF;
      char code_str[16];
      snprintf(code_str, sizeof(code_str), "%c%04X",
               "PCBU"[(first_byte >> 4) & 3], rest);

      detail_text += "DTC:       " + std::string(code_str) + "\n";
      detail_text += "Raw:       0x" + std::to_string(code) + "\n";
      detail_text += "Status:    0x" + std::to_string(dtc.status) + "\n";
      detail_text += "  TestFailed:          " +
                     std::string(dtc.status & 0x01 ? "Yes" : "No") + "\n";
      detail_text += "  TestThisCycle:       " +
                     std::string(dtc.status & 0x02 ? "Yes" : "No") + "\n";
      detail_text += "  Pending:             " +
                     std::string(dtc.status & 0x04 ? "Yes" : "No") + "\n";
      detail_text += "  Confirmed:           " +
                     std::string(dtc.status & 0x08 ? "Yes" : "No") + "\n";
      detail_text += "  TestNotSinceClear:   " +
                     std::string(dtc.status & 0x10 ? "Yes" : "No") + "\n";
      detail_text += "  TestFailedThisCycle: " +
                     std::string(dtc.status & 0x20 ? "Yes" : "No") + "\n";
      detail_text += "  WarningIndicator:    " +
                     std::string(dtc.status & 0x40 ? "Yes" : "No") + "\n";
    } else {
      detail_text = " Select a DTC to view details ";
    }

    auto detail_element =
        window(text(" DTC Detail "), paragraph(detail_text)) | flex;

    return hbox({dtc_list_element, separator(), detail_element}) | flex;
  });

  return renderer_;
}
