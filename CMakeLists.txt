cmake_minimum_required(VERSION 3.10)
project(detector-network-processor LANGUAGES CXX C)

string(TIMESTAMP PROJECT_DATE_STRING "%b %d, %Y")

option(PROJECT_DISABLE_SSL "build the code so the rest service doesn't use SSL"
       OFF)

set(PROJECT_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(PROJECT_HEADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(PROJECT_CONFIG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/config")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/output/bin")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/files.cmake")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -Wshadow -Wpedantic -Werror -O3)

find_library(MOSQUITTO mosquitto REQUIRED)
find_library(SASL2 sasl2 REQUIRED)
find_library(LDAP ldap REQUIRED)
find_library(DL dl REQUIRED)

find_package(
  Boost 1.71
  COMPONENTS system program_options
  REQUIRED)

set(PROJECT_INCLUDE_LIBS
    mosquitto
    pthread
    sasl2
    ldap
    boost_system
    boost_program_options
    ssl
    crypto
    dl)

# +++ necessary for compatability with older compilers
find_library(STD_CPP_FS stdc++fs /usr/lib/gcc/x86_64-linux-gnu/8/)

if(STD_CPP_FS)
  set(PROJECT_INCLUDE_LIBS ${PROJECT_INCLUDE_LIBS} stdc++fs)
endif()
# --- necessary for compatability with older compilers

configure_file("${PROJECT_CONFIG_DIR}/detector-network-processor.1"
               "${CMAKE_CURRENT_BINARY_DIR}/detector-network-processor.1")

configure_file("${PROJECT_CONFIG_DIR}/config.h"
               "${CMAKE_CURRENT_BINARY_DIR}/defaults.h")

add_executable(aggregation "${PROJECT_SRC_DIR}/aggregation.cpp")

add_executable(
  detector-network-processor ${PROJECT_SOURCE_FILES} ${PROJECT_HEADER_FILES})

target_include_directories(
  detector-network-processor PUBLIC ${PROJECT_HEADER_DIR} ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(detector-network-processor ${PROJECT_INCLUDE_LIBS})

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/packaging.cmake")

add_custom_target(clangformat COMMAND clang-format -style=WebKit -i ${PROJECT_SOURCE_FILES} ${PROJECT_HEADER_FILES} "${PROJECT_SRC_DIR}/aggregation.cpp")
