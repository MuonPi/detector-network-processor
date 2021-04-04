cmake_minimum_required(VERSION 3.10)
project(muondetector-cluster LANGUAGES CXX C)

string(TIMESTAMP PROJECT_DATE_STRING "%b %d, %Y")

option(CLUSTER_DISABLE_SSL "build the code so the rest service doesn't use SSL" OFF)

set(PROJECT_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(PROJECT_HEADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(PROJECT_CONFIG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/config")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/output/bin")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.cmake")


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
find_library(CRYPTOPP crypto++ REQUIRED)
find_library(SASL2 sasl2 REQUIRED)
find_library(LDAP ldap REQUIRED)

find_library(STD_CPP_FS stdc++fs /usr/lib/gcc/x86_64-linux-gnu/8/)


find_package(Boost 1.71 COMPONENTS system REQUIRED)



set(CLUSTER_SOURCE_FILES
    "${PROJECT_SRC_DIR}/coincidencefilter.cpp"
    "${PROJECT_SRC_DIR}/detectortracker.cpp"
    "${PROJECT_SRC_DIR}/detector.cpp"
    "${PROJECT_SRC_DIR}/triggerhandler.cpp"
    "${PROJECT_SRC_DIR}/application.cpp"

    "${PROJECT_SRC_DIR}/link/mqtt.cpp"
    "${PROJECT_SRC_DIR}/link/database.cpp"

    "${PROJECT_SRC_DIR}/messages/event.cpp"
    "${PROJECT_SRC_DIR}/messages/detectorlog.cpp"
    "${PROJECT_SRC_DIR}/messages/detectorinfo.cpp"
    "${PROJECT_SRC_DIR}/messages/detectorsummary.cpp"
    "${PROJECT_SRC_DIR}/messages/clusterlog.cpp"

    "${PROJECT_SRC_DIR}/utility/coincidence.cpp"
    "${PROJECT_SRC_DIR}/utility/eventconstructor.cpp"
    "${PROJECT_SRC_DIR}/utility/threadrunner.cpp"
    "${PROJECT_SRC_DIR}/utility/log.cpp"
    "${PROJECT_SRC_DIR}/utility/utility.cpp"
    "${PROJECT_SRC_DIR}/utility/parameters.cpp"
    "${PROJECT_SRC_DIR}/utility/configuration.cpp"
    "${PROJECT_SRC_DIR}/utility/resourcetracker.cpp"
    "${PROJECT_SRC_DIR}/utility/uppermatrix.cpp"
    "${PROJECT_SRC_DIR}/utility/restservice.cpp"
    "${PROJECT_SRC_DIR}/utility/scopeguard.cpp"

    "${PROJECT_SRC_DIR}/supervision/state.cpp"
    "${PROJECT_SRC_DIR}/supervision/timebase.cpp"
    )

set(CLUSTER_HEADER_FILES
    "${PROJECT_HEADER_DIR}/coincidencefilter.h"
    "${PROJECT_HEADER_DIR}/detectortracker.h"
    "${PROJECT_HEADER_DIR}/detector.h"
    "${PROJECT_HEADER_DIR}/triggerhandler.h"
    "${PROJECT_HEADER_DIR}/pipeline.h"
    "${PROJECT_HEADER_DIR}/application.h"

    "${PROJECT_HEADER_DIR}/link/database.h"
    "${PROJECT_HEADER_DIR}/link/mqtt.h"

    "${PROJECT_HEADER_DIR}/sink/base.h"
    "${PROJECT_HEADER_DIR}/sink/database.h"
    "${PROJECT_HEADER_DIR}/sink/mqtt.h"
    "${PROJECT_HEADER_DIR}/sink/ascii.h"

    "${PROJECT_HEADER_DIR}/source/base.h"
    "${PROJECT_HEADER_DIR}/source/mqtt.h"

    "${PROJECT_HEADER_DIR}/messages/event.h"
    "${PROJECT_HEADER_DIR}/messages/detectorlog.h"
    "${PROJECT_HEADER_DIR}/messages/detectorinfo.h"
    "${PROJECT_HEADER_DIR}/messages/detectorsummary.h"
    "${PROJECT_HEADER_DIR}/messages/clusterlog.h"
    "${PROJECT_HEADER_DIR}/messages/userinfo.h"
    "${PROJECT_HEADER_DIR}/messages/trigger.h"

    "${PROJECT_HEADER_DIR}/utility/threadrunner.h"
    "${PROJECT_HEADER_DIR}/utility/coincidence.h"
    "${PROJECT_HEADER_DIR}/utility/criterion.h"
    "${PROJECT_HEADER_DIR}/utility/log.h"
    "${PROJECT_HEADER_DIR}/utility/utility.h"
    "${PROJECT_HEADER_DIR}/utility/geohash.h"
    "${PROJECT_HEADER_DIR}/utility/eventconstructor.h"
    "${PROJECT_HEADER_DIR}/utility/parameters.h"
    "${PROJECT_HEADER_DIR}/utility/configuration.h"
    "${PROJECT_HEADER_DIR}/utility/resourcetracker.h"
    "${PROJECT_HEADER_DIR}/utility/uppermatrix.h"
    "${PROJECT_HEADER_DIR}/utility/restservice.h"
    "${PROJECT_HEADER_DIR}/utility/base64.h"
    "${PROJECT_HEADER_DIR}/utility/scopeguard.h"
    "${PROJECT_HEADER_DIR}/utility/exceptions.h"


    "${PROJECT_HEADER_DIR}/analysis/dataseries.h"
    "${PROJECT_HEADER_DIR}/analysis/cachedvalue.h"
    "${PROJECT_HEADER_DIR}/analysis/ratemeasurement.h"
    "${PROJECT_HEADER_DIR}/analysis/histogram.h"

    "${PROJECT_HEADER_DIR}/supervision/state.h"
    "${PROJECT_HEADER_DIR}/supervision/timebase.h"
    )

configure_file(
    "${PROJECT_CONFIG_DIR}/muondetector-cluster.1"
    "${CMAKE_CURRENT_BINARY_DIR}/muondetector-cluster.1"
    )

configure_file(
    "${PROJECT_CONFIG_DIR}/config.h"
    "${CMAKE_CURRENT_BINARY_DIR}/defaults.h"
    )

add_library(source-files OBJECT ${CLUSTER_SOURCE_FILES} ${CLUSTER_HEADER_FILES})
target_include_directories(source-files PUBLIC
    ${PROJECT_HEADER_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    )

add_executable(muondetector-cluster "${PROJECT_SRC_DIR}/main.cpp")

if (CMAKE_BUILD_TYPE STREQUAL Release)
    add_custom_command(TARGET muondetector-cluster POST_BUILD
            COMMAND ${CMAKE_STRIP} "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/muondetector-cluster")
endif ()
target_include_directories(muondetector-cluster PUBLIC
    ${PROJECT_HEADER_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    )

if (NOT STD_CPP_FS)
target_link_libraries(muondetector-cluster
    crypto++
    mosquitto
    source-files
    pthread
    sasl2
    ldap
    boost_system
    ssl
    crypto
    )
else()
target_link_libraries(muondetector-cluster
    crypto++
    mosquitto
    source-files
    pthread
    sasl2
    ldap
    stdc++fs
    boost_system
    ssl
    crypto
    )
endif()


include(GNUInstallDirs)

if (CMAKE_BUILD_TYPE STREQUAL Release)
add_custom_target(changelog-cluster ALL COMMAND gzip -cn9 "${PROJECT_CONFIG_DIR}/changelog" > "${CMAKE_CURRENT_BINARY_DIR}/changelog.gz")
add_custom_target(manpage-cluster ALL COMMAND gzip -cn9 "${CMAKE_CURRENT_BINARY_DIR}/muondetector-cluster.1" > "${CMAKE_CURRENT_BINARY_DIR}/muondetector-cluster.1.gz")
endif ()

add_custom_target(clangformat COMMAND clang-format -style=WebKit -i ${CLUSTER_SOURCE_FILES} ${CLUSTER_HEADER_FILES} "${PROJECT_SRC_DIR}/main.cpp")

install(TARGETS muondetector-cluster DESTINATION bin COMPONENT cluster)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/changelog.gz" DESTINATION "${CMAKE_INSTALL_DOCDIR}" COMPONENT cluster)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/muondetector-cluster.1.gz" DESTINATION "share/man/man1/" COMPONENT cluster)
install(FILES "${PROJECT_CONFIG_DIR}/copyright" DESTINATION "${CMAKE_INSTALL_DOCDIR}" COMPONENT cluster)
install(FILES "${PROJECT_CONFIG_DIR}/muondetector-cluster.service" DESTINATION "/lib/systemd/system" COMPONENT cluster)
install(FILES "${PROJECT_CONFIG_DIR}/muondetector-cluster.cfg" DESTINATION "/etc/muondetector/" COMPONENT cluster)
install(FILES "${PROJECT_CONFIG_DIR}/muondetector-cluster-credentials" DESTINATION "share/muondetector/" COMPONENT cluster)



set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_CONFIG_DIR}/license")
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${PROJECT_CONFIG_DIR}/postinst;${PROJECT_CONFIG_DIR}/conffiles")
set(CPACK_PACKAGE_VENDOR "MuonPi.org")
set(CPACK_DEBIAN_PACKAGE_SECTION "net")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/MuonPi/muondetector-cluster")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")
set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION " Daemon which calculates coincidences for incoming events
 It subscribes to a mqtt topic to collect the incoming event messages and keep
 trace of individual event sources.
 With these messages it calculates coincidences and depending on the context
 publishes them to another mqtt topic or writes them to a database.
 It is licensed under the GNU Lesser General Public License version 3 (LGPL v3).")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "MuonPi <developer@muonpi.org>")
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/output/packages/")

include(CPack)
