# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/home/pi/Drone/libs/MAVSDK/build/third_party/libmavlike/libmavlike/src/libmavlike")
  file(MAKE_DIRECTORY "/home/pi/Drone/libs/MAVSDK/build/third_party/libmavlike/libmavlike/src/libmavlike")
endif()
file(MAKE_DIRECTORY
  "/home/pi/Drone/libs/MAVSDK/build/third_party/libmavlike/libmavlike/src/libmavlike-build"
  "/home/pi/Drone/libs/MAVSDK/build/third_party/libmavlike/libmavlike"
  "/home/pi/Drone/libs/MAVSDK/build/third_party/libmavlike/libmavlike/tmp"
  "/home/pi/Drone/libs/MAVSDK/build/third_party/libmavlike/libmavlike/src/libmavlike-stamp"
  "/home/pi/Drone/libs/MAVSDK/build/third_party/libmavlike/libmavlike/src"
  "/home/pi/Drone/libs/MAVSDK/build/third_party/libmavlike/libmavlike/src/libmavlike-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/pi/Drone/libs/MAVSDK/build/third_party/libmavlike/libmavlike/src/libmavlike-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/pi/Drone/libs/MAVSDK/build/third_party/libmavlike/libmavlike/src/libmavlike-stamp${cfgdir}") # cfgdir has leading slash
endif()
