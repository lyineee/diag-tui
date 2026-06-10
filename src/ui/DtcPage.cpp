#include "ui/DtcPage.h"
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>
#include <spdlog/spdlog.h>

#include <sstream>

using namespace ftxui;

DtcPage::DtcPage(App& app) : app_(app) {}

ftxui::Component DtcPage::Build() {
  renderer_ = Renderer(std::function<Element()>([this] {
    auto& state = app_.GetState();
    std::lock_guard<std::mutex> lock(state.mtx);

    Elements dtc_elements;
    std::string detail_text;

    // Parse DTC response data stored in raw_response_text
    // When DTC response arrives, App parses and stores it
    // For now, show raw hex or a message
    if (state.last_raw_response.size() >= 4) {
      // Try to parse DTCs from raw response
      const auto& data = state.last_raw_response;
      size_t i = 0;
      int dtc_idx = 0;
      while (i + 3 <= data.size()) {
        uint32_t code = ((uint32_t)data[i] << 16) |
                        ((uint32_t)data[i + 1] << 8) | data[i + 2];
        uint8_t status = (i + 3 < data.size()) ? data[i + 3] : 0;

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
        if (status & 0x01) status_str += "testFailed ";
        if (status & 0x02) status_str += "testThisCycle ";
        if (status & 0x04) status_str += "pending ";
        if (status & 0x08) status_str += "confirmed ";
        if (status & 0x10) status_str += "testNotSinceClear ";
        if (status & 0x20) status_str += "testFailedThisCycle ";
        if (status & 0x40) status_str += "warning ";
        if (status_str.empty()) status_str = "none";

        std::string line = code_str + "  [" + status_str + "]";
        auto el = text(line);
        if (dtc_idx == selected_index_) el = el | bold | color(Color::Cyan) | bgcolor(Color::GrayDark);
        dtc_elements.push_back(el);

        // Show detail for selected DTC
        if (dtc_idx == selected_index_) {
          char code_str2[16];
          snprintf(code_str2, sizeof(code_str2), "%c%04X",
                   "PCBU"[(first_byte >> 4) & 3], rest);
          detail_text += "DTC:       " + std::string(code_str2) + "\n";
          detail_text += "Raw:       0x" + std::to_string(code) + "\n";
          detail_text += "Status:    0x" + std::to_string(status) + "\n";
          detail_text += "  TestFailed:          " + std::string(status & 0x01 ? "Yes" : "No") + "\n";
          detail_text += "  TestThisCycle:       " + std::string(status & 0x02 ? "Yes" : "No") + "\n";
          detail_text += "  Pending:             " + std::string(status & 0x04 ? "Yes" : "No") + "\n";
          detail_text += "  Confirmed:           " + std::string(status & 0x08 ? "Yes" : "No") + "\n";
          detail_text += "  TestNotSinceClear:   " + std::string(status & 0x10 ? "Yes" : "No") + "\n";
          detail_text += "  TestFailedThisCycle: " + std::string(status & 0x20 ? "Yes" : "No") + "\n";
          detail_text += "  WarningIndicator:    " + std::string(status & 0x40 ? "Yes" : "No") + "\n";
        }

        dtc_idx++;
        i += (i + 3 + 1 <= data.size()) ? 4 : 3;
      }
    }

    if (dtc_elements.empty()) {
      dtc_elements.push_back(text(" No DTCs found "));
    }

    auto dtc_list_element = window(text(" DTC List (F5: Refresh, F6: Clear, \u2191\u2193: Select) "),
                                    vbox(std::move(dtc_elements)) | vscroll_indicator | yframe) |
                            size(WIDTH, EQUAL, 50) | flex;

    if (detail_text.empty()) {
      detail_text = " Select a DTC to view details ";
    }

    auto detail_element =
        window(text(" DTC Detail "), paragraph(detail_text)) | flex;

    return hbox({dtc_list_element, separator(), detail_element}) | flex;
  }));

  return renderer_;
}

void DtcPage::Refresh() {
  app_.ReadDtc();
}
