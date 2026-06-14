#include "app/App.h"
#include "ui/StatusBar.h"
#include "ui/NavBar.h"
#include "ui/dtc/DtcPage.h"
#include "ui/did/DidPage.h"
#include "ui/RawPage.h"
#include "ui/SessionManager.h"
#include "ui/settings/SettingsPage.h"
#include "uds/DidDatabase.h"

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/component/event.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <memory>
#include <string>

using namespace ftxui;

int main() {
  auto file_logger = spdlog::basic_logger_mt("fuse-diag", "fuse-diag.log", true);
  spdlog::set_default_logger(file_logger);
  spdlog::set_level(spdlog::level::debug);
  spdlog::flush_on(spdlog::level::debug);
  spdlog::info("FuseDiag v0.1 starting...");

  App app;
  app.Init();

  auto screen = ScreenInteractive::Fullscreen();
  app.SetScreen(&screen);

  StatusBar status_bar(app);
  NavBar nav_bar(app);
  DtcPage dtc_page(app);
  DidPage did_page(app);
  RawPage raw_page(app);
  SessionManager session_manager(app);
  SettingsPage settings_page(app);

  auto status_component = status_bar.Build();
  auto nav_component = nav_bar.Build();
  auto dtc_component = dtc_page.Build();
  auto did_component = did_page.Build();
  auto raw_component = raw_page.Build();
  auto session_component = session_manager.Build();
  auto settings_component = settings_page.Build();

  NavPage current_page = NavPage::Dtc;
  app.GetState().current_page = current_page;

  nav_bar.SetOnPageChange([&](NavPage page) {
    current_page = page;
    app.GetState().current_page = page;
  });

  auto main_container = Container::Tab(
      {
          dtc_component,
          did_component,
          raw_component,
          session_component,
          settings_component,
      },
      (int*)&current_page);

  auto layout = Container::Horizontal({
      nav_component,
      main_container,
  });
  main_container->TakeFocus();

  auto full_layout = Container::Vertical({
      status_component,
      layout,
  });

  auto renderer = Renderer(full_layout, [&] {
    auto status_element = status_component->Render();
    auto body = layout->Render() | flex;
    return vbox({
               status_element,
               separator(),
               body | flex,
           }) |
           flex;
  });

  renderer |= CatchEvent([&](Event event) {
    if (app.HandleGlobalKeys(event)) return true;

    if (event == Event::Escape) {
      app.Disconnect();
      screen.Exit();
      return true;
    }

    if (event == Event::F5) {
      if (current_page == NavPage::Dtc) dtc_page.Refresh();
      return true;
    }

    if (event == Event::F6) {
      if (current_page == NavPage::Dtc) app.ClearDtc();
      return true;
    }

    if (event == Event::F2) {
      current_page = NavPage::Did;
      app.GetState().current_page = current_page;
      return true;
    }

    if (event == Event::F3) {
      current_page = NavPage::Raw;
      app.GetState().current_page = current_page;
      return true;
    }

    if (event == Event::Tab) {
      int next = ((int)current_page + 1) % (int)NavPage::COUNT_;
      current_page = (NavPage)next;
      app.GetState().current_page = current_page;
      return true;
    }

    return false;
  });

  screen.Loop(renderer);

  spdlog::info("FuseDiag shutting down");
  return 0;
}
