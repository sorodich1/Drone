# Install script for directory: /home/pi/Drone/libs/MAVSDK/src/mavsdk/core

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set path to fallback-tool for dependency-resolution.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  foreach(file
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libmavsdk.so.f740838b-dirty.f740838b-dirty.f740838b-dirty"
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libmavsdk.so.f740838b-dirty"
      )
    if(EXISTS "${file}" AND
       NOT IS_SYMLINK "${file}")
      file(RPATH_CHECK
           FILE "${file}"
           RPATH "")
    endif()
  endforeach()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE SHARED_LIBRARY FILES
    "/home/pi/Drone/libs/MAVSDK/build/src/mavsdk/libmavsdk.so.f740838b-dirty.f740838b-dirty.f740838b-dirty"
    "/home/pi/Drone/libs/MAVSDK/build/src/mavsdk/libmavsdk.so.f740838b-dirty"
    )
  foreach(file
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libmavsdk.so.f740838b-dirty.f740838b-dirty.f740838b-dirty"
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libmavsdk.so.f740838b-dirty"
      )
    if(EXISTS "${file}" AND
       NOT IS_SYMLINK "${file}")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/usr/bin/strip" "${file}")
      endif()
    endif()
  endforeach()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE SHARED_LIBRARY FILES "/home/pi/Drone/libs/MAVSDK/build/src/mavsdk/libmavsdk.so")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/mavsdk" TYPE FILE FILES
    "/home/pi/Drone/libs/MAVSDK/src/mavsdk/core/include/mavsdk/autopilot.h"
    "/home/pi/Drone/libs/MAVSDK/src/mavsdk/core/include/mavsdk/base64.h"
    "/home/pi/Drone/libs/MAVSDK/src/mavsdk/core/include/mavsdk/compatibility_mode.h"
    "/home/pi/Drone/libs/MAVSDK/src/mavsdk/core/include/mavsdk/component_type.h"
    "/home/pi/Drone/libs/MAVSDK/src/mavsdk/core/include/mavsdk/connection_result.h"
    "/home/pi/Drone/libs/MAVSDK/src/mavsdk/core/include/mavsdk/deprecated.h"
    "/home/pi/Drone/libs/MAVSDK/src/mavsdk/core/include/mavsdk/handle.h"
    "/home/pi/Drone/libs/MAVSDK/src/mavsdk/core/include/mavsdk/system.h"
    "/home/pi/Drone/libs/MAVSDK/src/mavsdk/core/include/mavsdk/mavsdk.h"
    "/home/pi/Drone/libs/MAVSDK/src/mavsdk/core/include/mavsdk/log_callback.h"
    "/home/pi/Drone/libs/MAVSDK/src/mavsdk/core/include/mavsdk/plugin_base.h"
    "/home/pi/Drone/libs/MAVSDK/src/mavsdk/core/include/mavsdk/server_plugin_base.h"
    "/home/pi/Drone/libs/MAVSDK/src/mavsdk/core/include/mavsdk/geometry.h"
    "/home/pi/Drone/libs/MAVSDK/src/mavsdk/core/include/mavsdk/server_component.h"
    "/home/pi/Drone/libs/MAVSDK/src/mavsdk/core/include/mavsdk/mavlink_address.h"
    "/home/pi/Drone/libs/MAVSDK/src/mavsdk/core/include/mavsdk/vehicle.h"
    "/home/pi/Drone/libs/MAVSDK/src/mavsdk/core/include/mavsdk/overloaded.h"
    "/home/pi/Drone/libs/MAVSDK/build/src/mavsdk/core/include/mavsdk/mavlink_include.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/mavsdk" TYPE DIRECTORY FILES "/home/pi/Drone/libs/MAVSDK/build/third_party/install/include/mavlink")
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/home/pi/Drone/libs/MAVSDK/build/src/mavsdk/core/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
