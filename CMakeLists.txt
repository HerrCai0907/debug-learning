cmake_minimum_required(VERSION 3.18)
project(debug-learn)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)

# include(FetchContent) FetchContent_Declare( googletest URL
# https://github.com/google/googletest/archive/15460959cbbfa20e66ef0b5ab497367e47fc0a04.zip
# ) set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
# FetchContent_MakeAvailable(googletest)

# enable_testing() include(GoogleTest)

if (APPLE)
    add_subdirectory(mac_ptrace)
endif()

if (UNIX)
    add_subdirectory(linux_int3)
endif()
