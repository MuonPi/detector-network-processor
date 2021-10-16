include(GNUInstallDirs)

if(CMAKE_BUILD_TYPE STREQUAL Release)
  add_custom_target(
    changelog-detector-network-processor ALL COMMAND gzip -cn9 "${PROJECT_CONFIG_DIR}/changelog" >
                                  "${CMAKE_CURRENT_BINARY_DIR}/changelog.gz")
  add_custom_target(
    manpage-detector-network-processor ALL
    COMMAND gzip -cn9 "${CMAKE_CURRENT_BINARY_DIR}/detector-network-processor.1" >
            "${CMAKE_CURRENT_BINARY_DIR}/detector-network-processor.1.gz")
  add_custom_command(
    TARGET detector-network-processor
    POST_BUILD
    COMMAND ${CMAKE_STRIP}
            "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/detector-network-processor")
endif()

install(
  TARGETS detector-network-processor
  DESTINATION bin
  COMPONENT detector-network-processor)
install(
  FILES "${CMAKE_CURRENT_BINARY_DIR}/changelog.gz"
  DESTINATION "${CMAKE_INSTALL_DOCDIR}"
  COMPONENT detector-network-processor)
install(
  FILES "${CMAKE_CURRENT_BINARY_DIR}/detector-network-processor.1.gz"
  DESTINATION "share/man/man1/"
  COMPONENT detector-network-processor)
install(
  FILES "${PROJECT_CONFIG_DIR}/copyright"
  DESTINATION "${CMAKE_INSTALL_DOCDIR}"
  COMPONENT detector-network-processor)
install(
  FILES "${PROJECT_CONFIG_DIR}/detector-network-processor.service"
  DESTINATION "/lib/systemd/system"
  COMPONENT detector-network-processor)
install(
  FILES "${PROJECT_CONFIG_DIR}/detector-network-processor.cfg"
  DESTINATION "/etc/muondetector/"
  COMPONENT detector-network-processor)

set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_CONFIG_DIR}/license")
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
    "${PROJECT_CONFIG_DIR}/postinst;${PROJECT_CONFIG_DIR}/conffiles")
set(CPACK_PACKAGE_VENDOR "MuonPi.org")
set(CPACK_DEBIAN_PACKAGE_SECTION "net")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE
    "https://github.com/MuonPi/detector-network-processor")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")

set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION
    " Daemon which calculates coincidences for incoming events
 It subscribes to a mqtt topic to collect the incoming event messages and keep
 trace of individual event sources.
 With these messages it calculates coincidences and depending on the context
 publishes them to another mqtt topic or writes them to a database.
 It is licensed under the GNU Lesser General Public License version 3 (LGPL v3)."
)
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "MuonPi <developer@muonpi.org>")
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/output/packages/")

include(CPack)
