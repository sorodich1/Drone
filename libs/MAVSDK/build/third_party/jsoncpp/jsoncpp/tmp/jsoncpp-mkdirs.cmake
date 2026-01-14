# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/home/pi/Drone/MAVSDK/build/third_party/jsoncpp/jsoncpp/src/jsoncpp")
  file(MAKE_DIRECTORY "/home/pi/Drone/MAVSDK/build/third_party/jsoncpp/jsoncpp/src/jsoncpp")
endif()
file(MAKE_DIRECTORY
  "/home/pi/Drone/MAVSDK/build/third_party/jsoncpp/jsoncpp/src/jsoncpp-build"
  "/home/pi/Drone/MAVSDK/build/third_party/jsoncpp/jsoncpp"
  "/home/pi/Drone/MAVSDK/build/third_party/jsoncpp/jsoncpp/tmp"
  "/home/pi/Drone/MAVSDK/build/third_party/jsoncpp/jsoncpp/src/jsoncpp-stamp"
  "/home/pi/Drone/MAVSDK/build/third_party/jsoncpp/jsoncpp/src"
  "/home/pi/Drone/MAVSDK/build/third_party/jsoncpp/jsoncpp/src/jsoncpp-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/pi/Drone/MAVSDK/build/third_party/jsoncpp/jsoncpp/src/jsoncpp-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/pi/Drone/MAVSDK/build/third_party/jsoncpp/jsoncpp/src/jsoncpp-stamp${cfgdir}") # cfgdir has leading slash
endif()
