set(TEST_SOURCES
  test_doip.cpp
  test_uds.cpp
  ${CMAKE_SOURCE_DIR}/src/uds/UdsMessage.cpp
  ${CMAKE_SOURCE_DIR}/src/uds/DidDatabase.cpp
)

add_executable(fuse-diag-tests ${TEST_SOURCES})

target_include_directories(fuse-diag-tests PRIVATE
  ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(fuse-diag-tests PRIVATE
  GTest::gtest
  GTest::gtest_main
  doip_lib
  uds-c
  nlohmann_json::nlohmann_json
  spdlog::spdlog
  Threads::Threads
)

add_test(NAME fuse-diag-tests COMMAND fuse-diag-tests)
