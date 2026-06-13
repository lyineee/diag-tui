# FuseDiag

Automotive UDS + DoIP diagnostic TUI tool. Connects to vehicle ECUs via DoIP (ISO 13400) over Ethernet, implements ISO 14229 (UDS) diagnostic services, rendered via [ftxui](https://github.com/ArthurSonzogni/FTXUI).

## Features

- **DoIP discovery & connection** — UDP broadcast discovery, TCP routing activation (ISO 13400-2)
- **DTC reading** — Read diagnostic trouble codes by status mask, detailed description lookup
- **DID read/write** — Read data identifiers, expand/collapse, live polling with graph view
- **Raw UDS send** — Send arbitrary hex payloads, inspect raw responses
- **Session management** — Switch between Default / Extended / Programming sessions, ECU reset, TesterPresent
- **Configurable** — Source address, target address, ECU IP via settings panel

## Build

```bash
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build -j$(nproc)
```

### Run

```bash
# TUI application
./build/fuse-diag

# Unit tests
./build/tests/fuse-diag-tests

# Local DoIP test server (port 13400)
./build/test-doip-server

# E2E tests (requires Node.js)
npm install
npm test
```

### Dependencies (auto-fetched via FetchContent)

| Dependency | Repo | Purpose |
|---|---|---|
| ftxui | ArthurSonzogni/FTXUI (v5.0.0) | TUI framework |
| doip-lib | langroodi/DoIP-Lib (master) | DoIP serialization (ISO 13400-2) |
| uds-c | openxc/uds-c (master) | UDS types (DiagnosticRequest, NRC enum) |
| nlohmann_json | nlohmann/json (v3.11.3) | JSON config parsing |
| spdlog | gabime/spdlog (v1.13.0) | File logging (fuse-diag.log) |
| googletest | google/googletest (v1.14.0) | Test framework |

## Architecture

```
src/
├── main.cpp                 # Entry point, ftxui event loop, keyboard shortcuts
├── app/
│   └── App.h/cpp            # Global state (AppState), orchestrates DoIP + UDS
├── doip/
│   ├── DoipTypes.h          # EcuInfo, DoipMessage structs, enums
│   └── DoipClient.h/cpp     # Async TCP/UDP client using DoIP-Lib
├── uds/
│   ├── UdsTypes.h           # DidEntry, DtcInfo, DiagResponse
│   ├── UdsMessage.h/cpp     # UDS request builder + response parser
│   ├── UdsClient.h/cpp      # UDS service wrappers (session, DTC, DID, etc.)
│   ├── DidDatabase.h/cpp    # DID metadata from JSON config
│   └── DtcDatabase.h/cpp    # DTC name/description lookup
├── ui/
│   ├── StatusBar.h/cpp      # Top bar: connection, SA/TA, session
│   ├── NavBar.h/cpp         # Left nav menu
│   ├── DtcPage.h/cpp        # DTC list + detail panel
│   ├── DidPage.h/cpp        # DID expandable list + polling controls
│   ├── DidItem.h/cpp        # Single DID expand/collapse component
│   ├── RawPage.h/cpp        # Hex raw message send/response
│   ├── SessionManager.h/cpp # Session switch, ECU reset, TesterPresent
│   └── SettingsPage.h/cpp   # IP/SA/TA config, connect/disconnect
│   └── dtc/                 # DTC UI module (component-based)
│       ├── DtcPage.h/cpp        # Tab menu + layout
│       ├── DtcListPanel.h/cpp   # DTC list + detail + buttons
│       └── DtcMaskFilter.h/cpp  # Collapsible status mask config
config/
├── did_database.json        # DID metadata definitions
└── dtc_database.json        # DTC name/description database
tests/
├── test_doip.cpp            # Unit tests (googletest)
├── test_uds.cpp
└── e2e/                     # End-to-end TUI tests (tui-test)
    ├── dtc.spec.ts           # DTC page test suite
    └── test-server.ts        # Test server lifecycle management
tools/
└── test_server.cpp          # DoIP server for local testing (UDP+TCP :13400)
```

## Keyboard Shortcuts

| Key | Action |
|---|---|
| `Tab` | Cycle through pages |
| `F2` | Jump to DID page |
| `F3` | Jump to Raw Send page |
| `F5` | Refresh DTC list |
| `F6` | Clear DTCs |
| `Escape` | Disconnect and exit |

## E2E Testing

End-to-end TUI tests use [microsoft/tui-test](https://github.com/microsoft/tui-test) to launch `fuse-diag` in a real terminal and simulate keystrokes. Tests run against a local `test-doip-server` instance.

```bash
# Install test dependencies (one time)
npm install

# Run all E2E tests
npm test

# Run only DTC page tests
npm run test:dtc
```

Test files are in `tests/e2e/` — written in TypeScript, using `test.describe` / `test` / `expect` API.

## Configuration

Edit JSON files in `config/` to customize:

- `did_database.json` — DID definitions (name, description, data size, graphable flag, polling interval)
- `dtc_database.json` — DTC code to human-readable name/description mapping

## Troubleshooting

- **Port in use**: `fuser -k 13400/tcp 13400/udp` to release
- **Logs**: Output goes to `fuse-diag.log`, not stdout
- **Test server**: Does not need root (port 13400 > 1024)

## License

MIT
