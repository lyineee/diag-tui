cmake_minimum_required(VERSION 3.20)
project(fuse-diag VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

include(FetchContent)

# ── Dependencies ──────────────────────────────────────────────────

FetchContent_Declare(
  ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
  GIT_TAG v5.0.0
)
FetchContent_MakeAvailable(ftxui)

FetchContent_Declare(
  doip-lib
  GIT_REPOSITORY https://github.com/langroodi/DoIP-Lib.git
  GIT_TAG master
)
FetchContent_MakeAvailable(doip-lib)

FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG v1.13.0
)
FetchContent_MakeAvailable(spdlog)

if(BUILD_TESTING)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
  )
  FetchContent_MakeAvailable(googletest)
endif()

find_package(Threads REQUIRED)

# ── Sources ────────────────────────────────────────────────────────

set(SOURCES
  src/main.cpp
  src/app/App.cpp
  src/ui/StatusBar.cpp
  src/ui/NavBar.cpp
  src/ui/DtcPage.cpp
  src/ui/DidPage.cpp
  src/ui/RawPage.cpp
  src/ui/SessionManager.cpp
  src/uds/UdsClient.cpp
  src/uds/DidDatabase.cpp
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
