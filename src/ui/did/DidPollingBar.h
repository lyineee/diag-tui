#pragma once

#include "app/App.h"
#include <ftxui/component/component.hpp>
#include <memory>
#include <vector>

struct DidEntry;
struct BoolFlag { bool val{false}; };

class DidPollingBar {
public:
  explicit DidPollingBar(App& app);
  ftxui::Component Build(std::shared_ptr<std::vector<BoolFlag>> poll_flags,
                          const std::vector<DidEntry>& dids);
  void CollapseAll(std::shared_ptr<std::vector<BoolFlag>> poll_flags,
                   const std::vector<DidEntry>& dids);

private:
  App& app_;
  std::string interval_{"3"};
};
