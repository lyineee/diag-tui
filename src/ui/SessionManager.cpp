#include "ui/SessionManager.h"
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <spdlog/spdlog.h>

using namespace ftxui;

SessionManager::SessionManager(App& app) : app_(app) {}

ftxui::Component SessionManager::Build() {
  btn_default_ = Button("Default (0x01)", [this] {
    app_.ChangeSession(0x01);
    status_text_ = "Switching to Default session...";
  });

  btn_extended_ = Button("Extended (0x03)", [this] {
    app_.ChangeSession(0x03);
    status_text_ = "Switching to Extended session...";
  });

  btn_programming_ = Button("Programming (0x02)", [this] {
    app_.ChangeSession(0x02);
    status_text_ = "Switching to Programming session...";
  });

  btn_tester_present_ = Button("Send TesterPresent", [this] {
    app_.GetUdsClient()->TesterPresent();
    status_text_ = "TesterPresent sent";
  });

  btn_reset_hard_ = Button("Hard Reset (0x01)", [this] {
    app_.GetUdsClient()->EcuReset(0x01);
    status_text_ = "Sending Hard Reset...";
  });

  btn_reset_soft_ = Button("Soft Reset (0x03)", [this] {
    app_.GetUdsClient()->EcuReset(0x03);
    status_text_ = "Sending Soft Reset...";
  });

  status_view_ = Renderer([this] {
    auto& state = app_.GetState();
    std::lock_guard<std::mutex> lock(state.mtx);
    std::string text = "Current Session: " + state.session_name + "\n";
    text += "Routing: " + std::string(state.routing_ok ? "Active" : "Inactive") + "\n";
    text += "Status: " + status_text_;
    return paragraph(text) | flex;
  });

  auto renderer = Renderer(
      Container::Vertical({
          Container::Horizontal({btn_default_, btn_extended_, btn_programming_}),
          Container::Horizontal({btn_tester_present_}),
          Container::Horizontal({btn_reset_hard_, btn_reset_soft_}),
          status_view_,
      }),
      std::function<Element()>([this] {
        return vbox({
                   window(text(" Session Control "),
                          vbox({
                              hbox({
                                  btn_default_->Render(),
                                  separator(),
                                  btn_extended_->Render(),
                                  separator(),
                                  btn_programming_->Render(),
                              }),
                          })),
                   separator(),
                   window(text(" Maintenance "),
                          vbox({
                              hbox({btn_tester_present_->Render()}),
                              separator(),
                              hbox({
                                  btn_reset_hard_->Render(),
                                  separator(),
                                  btn_reset_soft_->Render(),
                              }),
                          })),
                   separator(),
                   window(text(" Status "), status_view_->Render() | flex),
               }) |
               flex;
      }));

  return renderer;
}
