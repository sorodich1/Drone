# Install script for directory: /home/pi/Drone/libs/MAVSDK/src/mavsdk/plugins

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

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for each subdirectory.
  include("/home/pi/Drone/libs/MAVSDK/build/src/mavsdk/plugins/action/cmake_install.cmake")
  include("/home/pi/Drone/libs/MAVSDK/build/src/mavsdk/plugins/action_server/cmake_install.cmake")
  include("/home/pi/Drone/libs/MAVSDK/build/src/mavsdk/plugins/arm_authorizer_server/cmake_install.cmake")
  include("/home/pi/Drone/libs/MAVSDK/build/src/mavsdk/plugins/calibration/cmake_install.cmake")
  include("/home/pi/Drone/libs/MAVSDK/build/src/mavsdk/plugins/camera/cmake_install.cmake")
  include("/home/pi/Drone/libs/MAVSDK/build/src/mavsdk/plugins/camera_server/cmake_install.cmake")
  include("/home/pi/Drone/libs/MAVSDK/build/src/mavsdk/plugins/component_metadata/cmake_install.cmake")
  include("/home/pi/Drone/libs/MAVSDK/build/src/mavsdk/plugins/component_metadata_server/cmake_install.cmake")
  include("/home/pi/Drone/libs/MAVSDK/build/src/mavsdk/plugins/events/cmake_install.cmake")
  include("/home/pi/Drone/libs/MAVSDK/build/src/mavsdk/plugins/failure/cmake_install.cmake")
  include("/home/pi/Drone/libs/MAVSDK/build/src/mavsdk/plugins/follow_me/cmake_install.cmake")
  include("/home/pi/Drone/libs/MAVSDK/build/src/mavsdk/plugins/ftp/cmake_install.cmake")
  include("/home/pi/Drone/libs/MAVSDK/build/src/mavsdk/plugins/ftp_server/cmake_install.cmake")
  include("/home/pi/Drone/libs/MAVSDK/build/src/mavsdk/plugins/geofence/cmake_install.cmake")
  include("/home/pi/Drone/libs/MAVSDK/build/src/mavsdk/plugins/gimbal/cmake_install.cmake")
  include("/home/pi/Drone/libs/MAVSDK/build/src/mavsdk/plugins/gripper/cmake_install.cmake")
  include("/home/pi/Drone/libs/MAVSDK/build/src/mavsdk/plugins/info/cmake_install.cmake")
  include("/home/pi/Drone/libs/MAVSDK/build/src/mavsdk/plugins/log_files/cmake_install.cmake")
  include("/home/pi/Drone/libs/MAVSDK/build/src/mavsdk/plugins/log_streaming/cmake_install.cmake")
  include("/home/pi/Drone/libs/MAVSDK/build/src/mavsdk/plugins/manual_control/cmake_install.cmake")
  include("/home/pi/Drone/libs/MAVSDK/build/src/mavsdk/plugins/mavlink_direct/cmake_install.cmake")
  include("/home/pi/Drone/libs/MAVSDK/build/src/mavsdk/plugins/mission/cmake_install.cmake")
  include("/home/pi/Drone/libs/MAVSDK/build/src/mavsdk/plugins/mission_raw/cmake_install.cmake")
  include("/home/pi/Drone/libs/MAVSDK/build/src/mavsdk/plugins/mission_raw_server/cmake_install.cmake")
  include("/home/pi/Drone/libs/MAVSDK/build/src/mavsdk/plugins/mocap/cmake_install.cmake")
  include("/home/pi/Drone/libs/MAVSDK/build/src/mavsdk/plugins/offboard/cmake_install.cmake")
  include("/home/pi/Drone/libs/MAVSDK/build/src/mavsdk/plugins/param/cmake_install.cmake")
  include("/home/pi/Drone/libs/MAVSDK/build/src/mavsdk/plugins/param_server/cmake_install.cmake")
  include("/home/pi/Drone/libs/MAVSDK/build/src/mavsdk/plugins/rtk/cmake_install.cmake")
  include("/home/pi/Drone/libs/MAVSDK/build/src/mavsdk/plugins/server_utility/cmake_install.cmake")
  include("/home/pi/Drone/libs/MAVSDK/build/src/mavsdk/plugins/shell/cmake_install.cmake")
  include("/home/pi/Drone/libs/MAVSDK/build/src/mavsdk/plugins/telemetry/cmake_install.cmake")
  include("/home/pi/Drone/libs/MAVSDK/build/src/mavsdk/plugins/telemetry_server/cmake_install.cmake")
  include("/home/pi/Drone/libs/MAVSDK/build/src/mavsdk/plugins/transponder/cmake_install.cmake")
  include("/home/pi/Drone/libs/MAVSDK/build/src/mavsdk/plugins/tune/cmake_install.cmake")
  include("/home/pi/Drone/libs/MAVSDK/build/src/mavsdk/plugins/winch/cmake_install.cmake")
  include("/home/pi/Drone/libs/MAVSDK/build/src/mavsdk/plugins/mavlink_passthrough/cmake_install.cmake")

endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/home/pi/Drone/libs/MAVSDK/build/src/mavsdk/plugins/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
