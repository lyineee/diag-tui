#pragma once

#include <ftxui/component/component.hpp>
#include <memory>
#include <string>

class App;

class StatusBar {
public:
  explicit StatusBar(App& app);
  ftxui::Component Build();

private:
  void Update();

  App& app_;
  ftxui::Component renderer_;
  std::string status_text_;
  std::string ip_text_;
  std::string session_text_;
  bool connected_{false};
};
