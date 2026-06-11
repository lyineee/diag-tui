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
# Try find_package first; fall back to FetchContent if not found.

macro(fetch_or_find name)
  cmake_parse_arguments(_ARG "" "TAG;TARGET" "REPO;COMPONENTS" ${ARGN})
  if(NOT _ARG_TARGET)
    set(_ARG_TARGET "${name}")
  endif()
  if(TARGET ${_ARG_TARGET})
    return()
  endif()
  find_package(${name} QUIET)
  if(TARGET ${_ARG_TARGET})
    return()
  endif()
  FetchContent_Declare(
    ${name}
    GIT_REPOSITORY ${_ARG_REPO}
    GIT_TAG ${_ARG_TAG}
  )
  FetchContent_MakeAvailable(${name})
  if(NOT TARGET ${_ARG_TARGET})
    message(FATAL_ERROR "FetchContent for ${name} did not provide target ${_ARG_TARGET}")
  endif()
endmacro()

fetch_or_find(ftxui
  REPO https://github.com/ArthurSonzogni/FTXUI.git
  TAG v5.0.0
  TARGET ftxui::screen
)

fetch_or_find(doip-lib
  REPO https://github.com/langroodi/DoIP-Lib.git
  TAG master
  TARGET doip_lib
)

fetch_or_find(nlohmann_json
  REPO https://github.com/nlohmann/json.git
  TAG v3.11.3
  TARGET nlohmann_json::nlohmann_json
)

fetch_or_find(spdlog
  REPO https://github.com/gabime/spdlog.git
  TAG v1.13.0
  TARGET spdlog::spdlog
)

if(BUILD_TESTING)
  fetch_or_find(GTest
    REPO https://github.com/google/googletest.git
    TAG v1.14.0
    TARGET GTest::gtest
  )
endif()

# uds-c / isotp-c / bitfield-c (not commonly installed system-wide, always fetch)
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
