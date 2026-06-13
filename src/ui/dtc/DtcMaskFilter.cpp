#include "ui/dtc/DtcMaskFilter.h"
#include "uds/UdsTypes.h"
#include <ftxui/dom/elements.hpp>

#include <sstream>
#include <iomanip>

using namespace ftxui;

static std::string MaskHex(uint8_t m) {
  std::stringstream ss;
  ss << std::hex << (int)m;
  return ss.str();
}

Element DtcMaskFilter::Render() {
  if (!expanded) {
    return hbox({
      text(" Mask: 0x") | dim,
      text(MaskHex(mask)) | bold,
      text(" ") | dim,
      text("[m:configure]") | dim,
    });
  }

  const char* names[] = {
    "testFailed",
    "testFailedThisOperationCycle",
    "pendingDTC",
    "confirmedDTC",
    "testNotCompletedSinceLastClear",
    "testFailedSinceLastClear",
    "testNotCompletedThisOperationCycle",
    "warningIndicatorRequested"
  };

  Elements rows;
  for (int i = 0; i < 8; i++) {
    uint8_t bit = (uint8_t)(1 << i);
    bool on = mask & bit;
    auto cur = text(i == cursor ? "\u25B6 " : "   ");
    auto chk = text(on ? "[\u2713]" : "[ ]");
    if (on) chk = chk | bold;
    auto row = hbox({cur, chk, text(" " + std::string(names[i]))});
    if (i == cursor) row = row | inverted;
    rows.push_back(row);
  }
  rows.push_back(separator());
  rows.push_back(hbox({
    text(" Mask: 0x"), text(MaskHex(mask)) | bold,
    text("   "),
    text("jk:nav") | dim, text("  "),
    text("Space:toggle") | dim, text("  "),
    text("Enter:apply") | dim, text("  "),
    text("a:all") | dim, text("  "),
    text("r:invert") | dim, text("  "),
    text("m:collapse") | dim,
  }));

  return window(text(" DTC Status Mask "), vbox(std::move(rows)));
}

bool DtcMaskFilter::HandleEvent(Event event, uint8_t& mask_out) {
  if (!expanded) {
    if (event == Event::Character('m')) {
      expanded = true;
      return true;
    }
    return false;
  }

  if (event == Event::Character('j') || event == Event::ArrowDown)
    { cursor = (cursor + 1) % 8; return true; }
  if (event == Event::Character('k') || event == Event::ArrowUp)
    { cursor = (cursor + 7) % 8; return true; }
  if (event == Event::Character(' ')) {
    mask ^= (uint8_t)(1 << cursor);
    return true;
  }
  if (event == Event::Return) {
    mask_out = mask;
    expanded = false;
    return true;
  }
  if (event == Event::Character('r')) {
    mask ^= 0xFF;
    return true;
  }
  if (event == Event::Character('a')) {
    mask = 0xFF;
    return true;
  }
  if (event == Event::Character('m') || event == Event::Escape) {
    expanded = false;
    return true;
  }
  return false;
}
