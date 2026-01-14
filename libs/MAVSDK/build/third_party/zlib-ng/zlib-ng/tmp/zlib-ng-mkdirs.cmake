# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/home/pi/Drone/MAVSDK/build/third_party/zlib-ng/zlib-ng/src/zlib-ng")
  file(MAKE_DIRECTORY "/home/pi/Drone/MAVSDK/build/third_party/zlib-ng/zlib-ng/src/zlib-ng")
endif()
file(MAKE_DIRECTORY
  "/home/pi/Drone/MAVSDK/build/third_party/zlib-ng/zlib-ng/src/zlib-ng-build"
  "/home/pi/Drone/MAVSDK/build/third_party/zlib-ng/zlib-ng"
  "/home/pi/Drone/MAVSDK/build/third_party/zlib-ng/zlib-ng/tmp"
  "/home/pi/Drone/MAVSDK/build/third_party/zlib-ng/zlib-ng/src/zlib-ng-stamp"
  "/home/pi/Drone/MAVSDK/build/third_party/zlib-ng/zlib-ng/src"
  "/home/pi/Drone/MAVSDK/build/third_party/zlib-ng/zlib-ng/src/zlib-ng-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/pi/Drone/MAVSDK/build/third_party/zlib-ng/zlib-ng/src/zlib-ng-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/pi/Drone/MAVSDK/build/third_party/zlib-ng/zlib-ng/src/zlib-ng-stamp${cfgdir}") # cfgdir has leading slash
endif()
