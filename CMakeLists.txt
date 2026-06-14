cmake_minimum_required(VERSION 3.20)
project(fuse-diag VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

include(FetchContent)

# ── Dependencies ──────────────────────────────────────────────────
# Prefer system-installed libraries; fall back to FetchContent.

find_package(ftxui QUIET)
if(NOT ftxui_FOUND)
  FetchContent_Declare(ftxui GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git GIT_TAG v5.0.0)
  FetchContent_MakeAvailable(ftxui)
endif()

find_package(doip-lib QUIET)
if(NOT doip-lib_FOUND)
  FetchContent_Declare(doip-lib GIT_REPOSITORY https://github.com/langroodi/DoIP-Lib.git GIT_TAG master)
  FetchContent_MakeAvailable(doip-lib)
endif()

find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
  FetchContent_Declare(nlohmann_json GIT_REPOSITORY https://github.com/nlohmann/json.git GIT_TAG v3.11.3)
  FetchContent_MakeAvailable(nlohmann_json)
endif()

find_package(spdlog QUIET)
if(NOT spdlog_FOUND)
  FetchContent_Declare(spdlog GIT_REPOSITORY https://github.com/gabime/spdlog.git GIT_TAG v1.13.0)
  FetchContent_MakeAvailable(spdlog)
endif()

if(BUILD_TESTING)
  find_package(GTest QUIET)
  if(NOT GTest_FOUND)
    FetchContent_Declare(googletest GIT_REPOSITORY https://github.com/google/googletest.git GIT_TAG v1.14.0)
    FetchContent_MakeAvailable(googletest)
  endif()
endif()

find_package(Threads REQUIRED)

# ── Sources ────────────────────────────────────────────────────────

set(SOURCES
  src/main.cpp
  src/app/App.cpp
  src/ui/StatusBar.cpp
  src/ui/NavBar.cpp
  src/ui/dtc/DtcPage.cpp
  src/ui/dtc/DtcListPanel.cpp
  src/ui/dtc/DtcMaskFilter.cpp
  src/ui/dtc/DtcSnapshotPanel.cpp
  src/ui/DidPage.cpp
  src/ui/RawPage.cpp
  src/ui/SessionManager.cpp
  src/ui/SettingsPage.cpp
  src/ui/DidItem.cpp
  src/uds/UdsClient.cpp
  src/uds/DidDatabase.cpp
  src/uds/DtcDatabase.cpp
  src/uds/UdsMessage.cpp
  src/doip/DoipClient.cpp
)

add_executable(fuse-diag ${SOURCES})

target_include_directories(fuse-diag PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(fuse-diag PRIVATE
  ftxui::screen
  ftxui::dom
  ftxui::component
  doip_lib
  nlohmann_json::nlohmann_json
  spdlog::spdlog
  Threads::Threads
)

# ── Tests ─────────────────────────────────────────────────────────

option(BUILD_TESTING "Build tests" ON)
if(BUILD_TESTING)
  enable_testing()
  add_subdirectory(tests)
endif()

# Test DoIP server
add_executable(test-doip-server
  tools/test_server.cpp
  src/uds/UdsMessage.cpp
)
target_link_libraries(test-doip-server PRIVATE doip_lib)
target_include_directories(test-doip-server PRIVATE
  ${CMAKE_SOURCE_DIR}/src
)
