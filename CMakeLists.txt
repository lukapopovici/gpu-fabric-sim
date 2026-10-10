cmake_minimum_required(VERSION 3.16)
project(gpu_fabric_sim CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra -Wpedantic)

file(GLOB_RECURSE SIM_SOURCES CONFIGURE_DEPENDS src/*.cpp)
list(REMOVE_ITEM SIM_SOURCES ${CMAKE_SOURCE_DIR}/src/main.cpp)

add_library(fabric_sim_core ${SIM_SOURCES})
target_include_directories(fabric_sim_core PUBLIC include)

add_executable(gpu_fabric_sim src/main.cpp)
target_link_libraries(gpu_fabric_sim PRIVATE fabric_sim_core)

enable_testing()
add_subdirectory(tests)
