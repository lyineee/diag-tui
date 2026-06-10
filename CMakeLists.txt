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

# uds-c - C UDS library with ISO-TP transport (used for types + response parsing)
FetchContent_Declare(
  uds-c
  GIT_REPOSITORY https://github.com/openxc/uds-c.git
  GIT_TAG master
)
FetchContent_GetProperties(uds-c)
if(NOT uds-c_POPULATED)
  FetchContent_Populate(uds-c)
endif()

FetchContent_Declare(
  isotp-c
  GIT_REPOSITORY https://github.com/openxc/isotp-c.git
  GIT_TAG master
)
FetchContent_GetProperties(isotp-c)
if(NOT isotp-c_POPULATED)
  FetchContent_Populate(isotp-c)
endif()

FetchContent_Declare(
  bitfield-c
  GIT_REPOSITORY https://github.com/openxc/bitfield-c.git
  GIT_TAG master
)
FetchContent_GetProperties(bitfield-c)
if(NOT bitfield-c_POPULATED)
  FetchContent_Populate(bitfield-c)
endif()

set(UDS_C_INCLUDE
  ${uds-c_SOURCE_DIR}/src
  ${isotp-c_SOURCE_DIR}/src
  ${bitfield-c_SOURCE_DIR}/src
)

set(UDS_C_SOURCES
  ${uds-c_SOURCE_DIR}/src/uds/uds.c
  ${isotp-c_SOURCE_DIR}/src/isotp/isotp.c
  ${isotp-c_SOURCE_DIR}/src/isotp/send.c
  ${isotp-c_SOURCE_DIR}/src/isotp/receive.c
  ${isotp-c_SOURCE_DIR}/src/isotp/allocate.c
  ${bitfield-c_SOURCE_DIR}/src/bitfield/bitfield.c
  ${bitfield-c_SOURCE_DIR}/src/bitfield/8byte.c
  ${bitfield-c_SOURCE_DIR}/src/bitfield/bitarray.c
  ${bitfield-c_SOURCE_DIR}/src/canutil/read.c
  ${bitfield-c_SOURCE_DIR}/src/canutil/write.c
)

add_library(uds-c STATIC ${UDS_C_SOURCES})
target_include_directories(uds-c PUBLIC ${UDS_C_INCLUDE})
set_target_properties(uds-c PROPERTIES LINKER_LANGUAGE C)

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
  src/ui/SettingsPage.cpp
  src/uds/UdsClient.cpp
  src/uds/DidDatabase.cpp
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
  uds-c
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
target_link_libraries(test-doip-server PRIVATE doip_lib uds-c)
target_include_directories(test-doip-server PRIVATE
  ${CMAKE_SOURCE_DIR}/src
)
