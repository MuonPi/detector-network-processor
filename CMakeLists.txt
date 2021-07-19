cmake_minimum_required(VERSION 3.10)
project(detector-network-processor LANGUAGES CXX C)

string(TIMESTAMP PROJECT_DATE_STRING "%b %d, %Y")


option(PROCESSOR_BUILD_SANITIZERS "build the code with sanitizers"
       OFF)
option(PROCESSOR_DISABLE_SSL "build the code so the rest service doesn't use SSL"
       OFF)
option(PROCESSOR_BUILD_AGGREGATION "along with the default application, also build the aggregation executable."
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

if(PROCESSOR_BUILD_SANITIZERS)
add_compile_options(-fsanitize=address,undefined)
add_link_options(-fsanitize=address,undefined)
endif()


find_library(MOSQUITTO mosquitto REQUIRED)
find_library(DL dl REQUIRED)
find_library(MUONPICORE muonpi-core REQUIRED)
find_library(MUONPIMQTT muonpi-mqtt REQUIRED)

find_package(
  Boost 1.71
  COMPONENTS system program_options
  REQUIRED)

set(PROJECT_INCLUDE_LIBS
    mosquitto
    pthread
    ${Boost_LIBRARIES}
    ssl
    crypto
    dl
    muonpi-core
    muonpi-mqtt
    )

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

if (PROCESSOR_BUILD_AGGREGATION)
add_executable(aggregation "${PROJECT_SRC_DIR}/aggregation.cpp")
target_link_libraries(aggregation ${Boost_LIBRARIES})
endif()

add_executable(
  detector-network-processor ${PROJECT_SOURCE_FILES} ${PROJECT_HEADER_FILES})

target_include_directories(
  detector-network-processor PUBLIC ${PROJECT_HEADER_DIR} ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(detector-network-processor ${PROJECT_INCLUDE_LIBS})

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/packaging.cmake")

add_custom_target(clangformat COMMAND clang-format -style=WebKit -i ${PROJECT_SOURCE_FILES} ${PROJECT_HEADER_FILES} "${PROJECT_SRC_DIR}/aggregation.cpp")
