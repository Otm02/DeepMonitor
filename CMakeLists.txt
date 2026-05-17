cmake_minimum_required(VERSION 3.14)
project(predictive_maintenance_sensor_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Fetch Google Test ────────────────────────────────────────────────────────
include(FetchContent)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.14.0
)

FetchContent_Declare(
  rapidcheck
  GIT_REPOSITORY https://github.com/emil-e/rapidcheck.git
  GIT_TAG        master
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(RC_ENABLE_GTEST ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest rapidcheck)

# ── Source files (firmware modules, compiled for host) ───────────────────────
set(FIRMWARE_SOURCES
    src/imu.cpp
    src/normalizer.cpp
    src/window.cpp
    src/ml.cpp
    src/rtc.cpp
    src/nvs.cpp
    src/connectivity.cpp
    src/power.cpp
    src/flash_buffer.cpp
    src/main.cpp
)

file(GLOB_RECURSE TEST_SOURCES test/*.cpp)

add_executable(host_tests ${TEST_SOURCES} ${FIRMWARE_SOURCES})

target_include_directories(host_tests PRIVATE src)

target_compile_definitions(host_tests PRIVATE HOST_TEST)

target_link_libraries(host_tests
  PRIVATE
    GTest::gtest_main
    rapidcheck
    rapidcheck_gtest
)

include(GoogleTest)
gtest_discover_tests(host_tests)
