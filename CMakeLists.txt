cmake_minimum_required(VERSION 3.10)
project(muondetector-cluster LANGUAGES CXX C)

string(TIMESTAMP PROJECT_DATE_STRING "%b %d, %Y")

option(CLUSTER_DISABLE_SSL "build the code so the rest service doesn't use SSL" OFF)

set(PROJECT_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(PROJECT_HEADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(PROJECT_CONFIG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/config")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/output/bin")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/files.cmake")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


add_compile_options(
    -Wall
    -Wextra
    -Wshadow
    -Wpedantic
    -Werror
    -O3
    )



find_library(MOSQUITTO mosquitto REQUIRED)
find_library(SASL2 sasl2 REQUIRED)
find_library(LDAP ldap REQUIRED)
find_library(DL dl REQUIRED)

find_library(STD_CPP_FS stdc++fs /usr/lib/gcc/x86_64-linux-gnu/8/)


find_package(Boost 1.71 COMPONENTS system program_options REQUIRED)

configure_file(
    "${PROJECT_CONFIG_DIR}/muondetector-cluster.1"
    "${CMAKE_CURRENT_BINARY_DIR}/muondetector-cluster.1"
    )

configure_file(
    "${PROJECT_CONFIG_DIR}/config.h"
    "${CMAKE_CURRENT_BINARY_DIR}/defaults.h"
    )

add_executable(aggregation "${PROJECT_SRC_DIR}/aggregation.cpp")

add_executable(muondetector-cluster ${CLUSTER_SOURCE_FILES} ${CLUSTER_HEADER_FILES} "${PROJECT_SRC_DIR}/main.cpp")


target_include_directories(muondetector-cluster PUBLIC
    ${PROJECT_HEADER_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    )

set(CLUSTER_INCLUDE_LIBS
    mosquitto
    source-files
    pthread
    sasl2
    ldap
    boost_system
    boost_program_options
    ssl
    crypto
    dl
    )

if (STD_CPP_FS)
set(CLUSTER_INCLUDE_LIBS
    ${CLUSTER_INCLUDE_LIBS}
    stdc++fs
    )
endif()

target_link_libraries(muondetector-cluster
    ${CLUSTER_INCLUDE_LIBS}
    )


include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/packaging.cmake")
