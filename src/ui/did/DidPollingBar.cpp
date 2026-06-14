#include "ui/did/DidPollingBar.h"
#include "uds/DidDatabase.h"
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>

using namespace ftxui;

DidPollingBar::DidPollingBar(App& app) : app_(app) {
  interval_ = std::to_string(DidDatabase::Instance().GetPollInterval());
}

void DidPollingBar::CollapseAll(std::shared_ptr<std::vector<BoolFlag>> poll_flags,
                                 const std::vector<DidEntry>& dids) {
  for (auto& b : *poll_flags) b.val = false;
  for (auto& e : dids) DidDatabase::Instance().SetExpanded(e.did, false);
  DidDatabase::Instance().Save("config/did_database.json");
}

Component DidPollingBar::Build(std::shared_ptr<std::vector<BoolFlag>> poll_flags,
                                const std::vector<DidEntry>& dids) {
  auto input_interval = Input(&interval_, " sec");
  auto btn_poll = Button("Toggle", [this, poll_flags, &dids] {
    bool do_stop = false;
    std::vector<uint16_t> poll_dids;
    int interval = 3;
    {
      auto& state = app_.GetState();
      std::lock_guard<std::recursive_mutex> lock(state.mtx);
      if (state.polling_active) {
        do_stop = true;
      } else {
        for (size_t i = 0; i < dids.size() && i < poll_flags->size(); i++)
          if ((*poll_flags)[i].val) poll_dids.push_back(dids[i].did);
        try { interval = std::stoi(interval_); } catch (...) {}
        if (interval < 1) interval = 1;
      }
    }
    DidDatabase::Instance().SetPollInterval(interval);
    DidDatabase::Instance().Save("config/did_database.json");
    if (do_stop) app_.StopPolling();
    else if (!poll_dids.empty()) app_.StartPolling(poll_dids, interval);
  });

  auto btn_collapse = Button(" Collapse All ", [this, poll_flags, &dids] {
    CollapseAll(poll_flags, dids);
  });

  auto renderer = Renderer([this, input_interval, btn_poll, btn_collapse, poll_flags] {
    auto& state = app_.GetState();
    std::lock_guard<std::recursive_mutex> lock(state.mtx);
    int expanded = 0;
    for (auto& b : *poll_flags) if (b.val) expanded++;
    return hbox({
      text(" Interval: ") | bold,
      input_interval->Render() | size(WIDTH, EQUAL, 4),
      text(" s "),
      btn_poll->Render(),
      separator(),
      text(state.polling_active ? " Active " : " Idle ") |
          (state.polling_active ? color(Color::GreenLight) : color(Color::GrayDark)),
      separator(),
      text(std::to_string(expanded) + " DID(s) monitored ") | dim,
      separator(),
      btn_collapse->Render(),
    });
  });

  return Renderer(Container::Horizontal({input_interval, btn_poll, btn_collapse}),
    [this, input_interval, btn_poll, btn_collapse, renderer] {
      return renderer->Render();
    });
}
