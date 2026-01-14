#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "mav::mav" for configuration "Release"
set_property(TARGET mav::mav APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(mav::mav PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libmav.a"
  )

list(APPEND _cmake_import_check_targets mav::mav )
list(APPEND _cmake_import_check_files_for_mav::mav "${_IMPORT_PREFIX}/lib/libmav.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
