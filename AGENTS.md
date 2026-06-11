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
| uds-c | openxc/uds-c (master) | UDS types (DiagnosticResponse, NRC enum) |
| nlohmann_json | nlohmann/json (v3.11.3) | JSON parsing for config files |
| spdlog | gabime/spdlog (v1.13.0) | File logging (outputs to fuse-diag.log) |
| googletest | google/googletest (v1.14.0) | Test framework |

## Architecture

```
src/
├── main.cpp                 # Entry point, ftxui event loop, keyboard shortcuts
├── app/
│   ├── App.h/cpp            # Global state (AppState), orchestrates DoIP + UDS
├── doip/
│   ├── DoipTypes.h          # EcuInfo, DoipMessage structs, enums
│   └── DoipClient.h/cpp     # Async TCP/UDP client using DoIP-Lib
├── uds/
│   ├── UdsTypes.h           # DidEntry, DtcInfo
│   ├── UdsMessage.h/cpp     # Request builder + response parser (uses uds-c types)
│   ├── UdsClient.h/cpp      # UDS service wrappers (session, DTC, DID, etc.)
│   └── DidDatabase.h/cpp    # DID metadata from JSON config
├── ui/
│   ├── StatusBar.h/cpp      # Top bar: connection, SA/TA, session
│   ├── NavBar.h/cpp         # Left nav menu (DTC/DID/Raw/Session/Settings)
│   ├── DtcPage.h/cpp        # DTC list + detail panel
│   ├── DidPage.h/cpp        # DID expandable list + polling controls
│   ├── DidItem.h/cpp        # Single DID expand/collapse component
│   ├── RawPage.h/cpp        # Hex raw message send/response
│   ├── SessionManager.h/cpp # Session switch, ECU reset, TesterPresent
│   └── SettingsPage.h/cpp   # IP/SA/TA config, connect/disconnect
tools/
└── test_server.cpp          # DoIP server for local testing (UDP+TCP :13400)
```

## Key Patterns

### Async Networking
All network I/O runs on background threads. DoipClient uses `std::thread` for TCP receive and UDP discovery. Connection is managed via `AsyncConnect()` with a `ConnectCallback`. No blocking calls on the ftxui main thread.

### Thread Safety
`AppState::mtx` is `std::recursive_mutex` (supports nested locks from same thread during rendering). UI renderers lock `state.mtx`; background threads also lock when updating state.

### UDS Request/Response
UDS request building and response parsing go through `UdsMessage` (in `src/uds/`), which uses `DiagnosticRequest` / `DiagnosticResponse` types from the `uds-c` library. Raw UDS bytes travel over DoIP's `DiagMessage` payload.

### Polling
Background thread in `App::PollingThread()` periodically calls `ReadDid()` for expanded DIDs. Results accumulate in `AppState::did_history` (max 120 points) for graph rendering.

## Conventions

- **No comments in code** unless explicitly required
- **C++17**, RAII, smart pointers
- ftxui components: prefer standard components (Button, Renderer, Container) over custom ComponentBase subclasses to avoid focus management issues
- Use `std::make_shared` for component ownership
- All UI pages expose `Build()` → `ftxui::Component`
- UDP/TCP on port **13400** (DoIP standard)

## Troubleshooting

- **Toggle freeze**: Ensure DidItem uses standard Button + Renderer, no custom OnEvent/Focusable override
- **spdlog interference**: Logs go to `fuse-diag.log`, not stdout
- **Port in use**: `fuser -k 13400/tcp 13400/udp` to release
- **Test server**: `./build/test-doip-server` (does NOT need root, port 13400 is >1024)
