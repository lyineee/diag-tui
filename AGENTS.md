# FuseDiag — Agent Guide

## Project

Automotive UDS + DoIP diagnostic TUI tool. Connects to vehicle ECUs via DoIP (ISO 13400) over Ethernet, implements ISO 14229 (UDS) diagnostic services, rendered via ftxui.

## Build & Run

```bash
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build -j$(nproc)
./build/fuse-diag                     # TUI application
./build/tests/fuse-diag-tests         # unit tests
./build/test-doip-server              # local test server (port 13400)
```

## Dependencies (FetchContent)

| Dependency | Repo | Purpose |
|---|---|---|
| ftxui | ArthurSonzogni/FTXUI (v5.0.0) | TUI framework |
| doip-lib | langroodi/DoIP-Lib (master) | DoIP serialization (ISO 13400-2) |
| nlohmann_json | nlohmann/json (v3.11.3) | JSON config parsing |
| spdlog | gabime/spdlog (v1.13.0) | File logging (fuse-diag.log) |
| googletest | google/googletest (v1.14.0) | Test framework |

UDS types (`DiagnosticRequest`, `DiagnosticNegativeResponseCode`) are inlined in `src/uds/UdsTypes.h` — the uds-c/isop-c/bitfield-c libraries are no longer fetched or built.

## Architecture

```
src/
├── main.cpp                 # Entry point, ftxui event loop, keyboard shortcuts
├── app/
│   └── App.h/cpp            # Global state (AppState), orchestrates DoIP + UDS
├── doip/
│   ├── DoipTypes.h          # EcuInfo, DoipMessage structs, enums
│   └── DoipClient.h/cpp     # Async TCP/UDP client, status change callbacks
├── uds/
│   ├── UdsTypes.h           # DidEntry, DtcInfo, DiagResponse, UDS enums
│   ├── UdsMessage.h/cpp     # UDS request builder + response parser
│   ├── UdsClient.h/cpp      # UDS service wrappers (session, DTC, DID, etc.)
│   ├── DidDatabase.h/cpp    # DID metadata from JSON config
│   └── DtcDatabase.h/cpp    # DTC name/description lookup
├── ui/
│   ├── StatusBar.h/cpp      # Top bar: connection, SA/TA, session
│   ├── NavBar.h/cpp         # Left nav menu
│   ├── DidPage.h/cpp        # DID expandable list + polling controls
│   ├── DidItem.h/cpp        # Single DID expand/collapse component
│   ├── RawPage.h/cpp        # Hex raw message send/response
│   ├── SessionManager.h/cpp # Session switch, ECU reset, TesterPresent
│   ├── SettingsPage.h/cpp   # IP/SA/TA config, connect/disconnect
│   └── dtc/                 # DTC UI module (component-based)
│       ├── DtcPage.h/cpp        # Tab menu + layout
│       ├── DtcListPanel.h/cpp   # DTC list + detail + buttons
│       └── DtcMaskFilter.h/cpp  # Collapsible status mask config
config/
├── did_database.json        # DID metadata
└── dtc_database.json        # DTC name/description database
tools/
└── test_server.cpp          # DoIP server for local testing (UDP+TCP :13400)
```

## Key Patterns

### Component-Based UI Design

Each UI module is split into focused classes, each under 200 lines. A complex page like DTC is decomposed into:

| Class | File | Responsibility |
|---|---|---|
| `DtcPage` | `ui/dtc/DtcPage.h/cpp` | Tab menu + `Container::Tab` switching |
| `DtcListPanel` | `ui/dtc/DtcListPanel.h/cpp` | DTC list rendering, keyboard navigation, event dispatch |
| `DtcMaskFilter` | `ui/dtc/DtcMaskFilter.h/cpp` | Status mask configuration with Checkboxes + Buttons |

Each class exposes a `Build()` method returning `ftxui::Component`. This keeps files small, responsibilities clear, and components independently testable.

### Prefer ftxui Native Components

Always use standard ftxui components. Avoid creating custom `ComponentBase` subclasses or overriding `OnEvent()`/`Focusable()` manually.

| Requirement | Use |
|---|---|
| Interactive list | `Container::Vertical` with focusable child components |
| Tab switching | `Menu` + `Container::Tab` |
| Toggle on/off | `Checkbox` |
| Click action | `Button` (with `ButtonOption::Ascii()`) |
| Custom rendering with events | `Renderer([](bool) -> Element { ... })` (focusable) |
| Inline event handling | `CatchEvent` decorator |
| Conditional visibility | Render conditionally in the `Renderer` lambda (e.g. `if (expanded) return ...;`) |

Only use custom `ComponentBase` subclasses when absolutely necessary — and even then, prefer composing existing components.

### Renderer Focusability

`Renderer` has two overloads:

```cpp
Renderer([] { ... })              // NON-focusable — only for display
Renderer([](bool focused) { ... }) // FOCUSABLE — mouse click triggers TakeFocus()
```

Always use the focusable variant for interactive content (lists, panels with keyboard shortcuts).
The focus chain (`TakeFocus()`) is how ftxui routes keyboard events; without it, Arrow keys and shortcuts
won't reach your component.

### CatchEvent Best Practices

Apply `CatchEvent` **locally** on the component that owns the events, not at a high level.

```cpp
auto my_panel = Renderer([this](bool) -> Element { ... });
my_panel |= CatchEvent([this](Event event) {
    // Handle events specific to this panel
    if (event == Event::ArrowDown) { ...; return true; }   // consumed
    if (event == Event::ArrowUp)   { ...; return true; }   // consumed
    return false;  // pass through to children
});
```

**Return strategy:**
- Return `true` — the event is consumed; child components won't see it
- Return `false` — the event passes through to child components (Button, Checkbox, etc.)

**Bad (blocks child components):**
```cpp
// Don't put all event handling at the top level
outer_renderer |= CatchEvent([&](Event e) {
    if (e == Event::ArrowDown) { ...; return true; }  // blocks Button clicks!
    return false;
});
```

**Good (local, only on the component that needs it):**
```cpp
// Each component handles its own events
list_panel |= CatchEvent(...);   // handles list navigation
mask_panel |= CatchEvent(...);   // handles mask shortcuts
// Buttons work naturally without interference
```

### Focus Chain & Maybe

ftxui's `Container::Vertical` / `Container::Horizontal` only cycle focus through **focusable** children.
Non-focusable children are skipped. Hidden components (collapsed panels, invisible wrappers)
must be excluded from the focus chain or they become "ghost" stops during Tab navigation.

**Use `Maybe` to conditionally exclude components from the focus chain:**

```cpp
auto show = [this] { return expanded; };
auto panel = Maybe(my_panel, show);
auto alt_btn = Maybe(my_button, [this] { return !expanded; });

// Only one set of components is focusable at a time
Container::Vertical({panel, alt_btn});
```

`Maybe` calls the condition function on every event to decide whether the child should
receive focus. When hidden, `Focusable()` returns `false` and the component is invisible
to keyboard navigation. The child still renders normally — control visibility in the
`Renderer` lambda separately.

**Never put `Renderer(..., [] { return text(""); })` as a bare child of Container::Vertical.**
This creates an invisible-but-focusable component that stops keyboard navigation.
Always wrap with `Maybe` or exclude entirely.

**For inline-rendered components (buttons rendered in a parent Render's lambda):**
keep the real `Button` components in the Container tree for focus/event support,
but suppress their visual output with a wrapping Renderer that returns empty text.
This way the buttons are Tab-able but their visual rendering comes from the parent.

```cpp
auto btn_bar = Container::Horizontal({btn1, btn2});
auto invisible = Renderer(btn_bar, [] { return text(""); });
// invisible suppresses visual output, but Buttons remain focusable
```

**Container::Vertical's `MoveSelectorDown` fires BEFORE forwarding the event to the active child.** Once the selector moves, the event goes to the NEW active child — the previous child never sees it. This means pressing ArrowDown when component A has focus will move focus to component B, and component A's CatchEvent/OnEvent won't receive the ArrowDown. This is by design.

**Focus wraps around.** At the last child, ArrowDown wraps to the first child; at the first child, ArrowUp wraps to the last. This is how ftxui's Container::Vertical cycles focus.

**Example: DtcMaskFilter collapsible panel with `Maybe`**

When the mask is collapsed, 8 `Checkbox` components and the action `Button` bar are hidden via `Maybe`:
```cpp
auto show_expanded = [this] { return expanded; };
auto show_collapsed = [this] { return !expanded; };
auto check_list_vis = Maybe(check_list, show_expanded);   // hidden when collapsed
auto btn_bar_vis    = Maybe(btn_bar, show_expanded);      // hidden when collapsed
auto btn_expand_vis = Maybe(btn_expand, show_collapsed); // visible only when collapsed

Container::Vertical({check_list_vis, btn_bar_vis, btn_expand_vis});
```

When collapsed, only `btn_expand_vis` is focusable. When expanded, only `check_list_vis` and `btn_bar_vis` are focusable. No empty stops in the focus chain. This is the correct pattern for any component that toggles between visible/interactive states.

### Lambda Capture Safety

Avoid `[&]` in lambdas that outlive the current scope (e.g. `Renderer` or `Button` callbacks in `Build()`).
Local variables in `Build()` are destroyed when the function returns — capturing them by reference causes **undefined behavior / hangs**.

```cpp
// BAD — [&] captures dangling references after Build() returns
auto r = Renderer(..., [&] { ... });

// GOOD — [=] captures by value; [this] for member access
auto r = Renderer(..., [=] { ... });
auto btn = Button("...", [this] { ... });
```

When you need a shared helper lambda, capture it by value in the callback:

```cpp
auto helper = [this] { do_work(); };
auto btn = Button("...", [this, helper] { helper(); });
```

### Async Networking

All network I/O runs on background threads. DoipClient uses `std::thread` for TCP receive and UDP discovery. Connection is managed via `AsyncConnect()` with a `ConnectCallback`. No blocking calls on the ftxui main thread.

### Thread Safety

`AppState::mtx` is `std::recursive_mutex` (supports nested locks from same thread during rendering). UI renderers lock `state.mtx`; background threads also lock when updating state.

### UDS Request/Response

UDS request building and response parsing go through `UdsMessage` (in `src/uds/`). Raw UDS bytes travel over DoIP's `DiagMessage` payload. Responses are parsed into `DiagResponse` with unbounded `std::vector<uint8_t>` payload.

### Polling

Background thread in `App::PollingThread()` periodically calls `ReadDid()` for expanded DIDs. Results accumulate in `AppState::did_history` (max 120 points) for graph rendering.

### Screen Refresh

When background data arrives (UDS response, connection status change), call `screen_->PostEvent(ftxui::Event::Custom)` to force an immediate TUI re-render. Without this, the screen only updates on user input.

## Conventions

- **No comments in code** unless explicitly required
- **C++17**, RAII, smart pointers
- **Component-based**: each UI class ≤ 200 lines, single responsibility, `Build() → ftxui::Component`
- **Prefer standard components**: Button, Checkbox, Container, Renderer, Menu — no custom ComponentBase subclasses
- **Local CatchEvent**: apply on the specific component that needs it, not globally
- **Lambda safety**: use `[=]` or `[this]`, never `[&]` in `Build()` lambdas
- **Focusable Renderer**: use `Renderer([](bool) { ... })` for interactive content
- All UI pages expose `Build()` → `ftxui::Component`
- UDP/TCP on port **13400** (DoIP standard)

## Troubleshooting

- **Component not responding to keys**: Check that the Renderer is focusable (`[](bool)` variant) and `CatchEvent` is on the right component. Ensure `CatchEvent` returns `false` for events that child components should handle
- **Keyboard Tab skips or lands on invisible items**: Hidden or empty components in `Container::Vertical`/`Horizontal` still appear in the focus chain. Use `Maybe` to conditionally exclude them
- **Program hangs on start/interaction**: Check for `[&]` captures in `Build()` lambdas — local variables become dangling references
- **Toggle freeze**: Use standard Button + Checkbox components, no custom OnEvent/Focusable override
- **spdlog interference**: Logs go to `fuse-diag.log`, not stdout
- **Port in use**: `fuser -k 13400/tcp 13400/udp` to release
- **Test server**: `./build/test-doip-server` (does NOT need root, port 13400 is >1024)
- **UI not refreshing after background update**: Call `screen_->PostEvent(ftxui::Event::Custom)` in the data callback

## Testing Methodology

### E2E Tests (tui-test)

Automated TUI tests using [microsoft/tui-test](https://github.com/microsoft/tui-test). Tests launch `fuse-diag` in a real terminal pty and simulate keystrokes. Best suited for **special keys** (Tab, F2-F12, Escape, Arrow keys) and **content rendering assertions**.

```bash
npm install              # one-time setup
npm test                 # run all E2E tests
```

Character keys (`m`, `j`, `k`, `a`, `r`) may not reach ftxui's event loop reliably through the pty. For those, first verify manually with terminal MCP, then automate if stable.

### Terminal MCP (Visual Verification)

Use terminal MCP to interactively verify TUI behavior. `sendKey` supports special keys; `type` sends character input when the correct component has focus.

**Workflow:**
1. Start terminal MCP session with `test-doip-server` and `fuse-diag`
2. Execute keyboard navigation step by step
3. `takeScreenshot(format: "text")` at each step to trace focus
4. When a Button shows `[Label]` (brackets = focused), interact with it
5. Convert verified sequences to E2E tests

**Example: mask expansion**
```
ArrowRight  → focus enters DTC content area
ArrowDown   → through mask → list renderer
ArrowDown   → wraps to Configure (m) button [shows brackets]
type("m")   → mask expands with 8 checkboxes
```

**Debugging focus issues:**
Use screenshots to trace focus position. A focused `Button` renders as `[Label]` (with brackets, Ascii style).