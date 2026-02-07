# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/home/pi/Drone/libs/MAVSDK/build/third_party/liblzma/liblzma-prefix/src/liblzma")
  file(MAKE_DIRECTORY "/home/pi/Drone/libs/MAVSDK/build/third_party/liblzma/liblzma-prefix/src/liblzma")
endif()
file(MAKE_DIRECTORY
  "/home/pi/Drone/libs/MAVSDK/build/third_party/liblzma/liblzma-prefix/src/liblzma-build"
  "/home/pi/Drone/libs/MAVSDK/build/third_party/liblzma/liblzma-prefix"
  "/home/pi/Drone/libs/MAVSDK/build/third_party/liblzma/liblzma-prefix/tmp"
  "/home/pi/Drone/libs/MAVSDK/build/third_party/liblzma/liblzma-prefix/src/liblzma-stamp"
  "/home/pi/Drone/libs/MAVSDK/build/third_party/liblzma/liblzma-prefix/src"
  "/home/pi/Drone/libs/MAVSDK/build/third_party/liblzma/liblzma-prefix/src/liblzma-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/pi/Drone/libs/MAVSDK/build/third_party/liblzma/liblzma-prefix/src/liblzma-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/pi/Drone/libs/MAVSDK/build/third_party/liblzma/liblzma-prefix/src/liblzma-stamp${cfgdir}") # cfgdir has leading slash
endif()
