#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "GTest::gtest" for configuration "Release"
set_property(TARGET GTest::gtest APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(GTest::gtest PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libgtest.so.1.14.0"
  IMPORTED_SONAME_RELEASE "libgtest.so.1.14.0"
  )

list(APPEND _cmake_import_check_targets GTest::gtest )
list(APPEND _cmake_import_check_files_for_GTest::gtest "${_IMPORT_PREFIX}/lib/libgtest.so.1.14.0" )

# Import target "GTest::gtest_main" for configuration "Release"
set_property(TARGET GTest::gtest_main APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(GTest::gtest_main PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libgtest_main.so.1.14.0"
  IMPORTED_SONAME_RELEASE "libgtest_main.so.1.14.0"
  )

list(APPEND _cmake_import_check_targets GTest::gtest_main )
list(APPEND _cmake_import_check_files_for_GTest::gtest_main "${_IMPORT_PREFIX}/lib/libgtest_main.so.1.14.0" )

# Import target "GTest::gmock" for configuration "Release"
set_property(TARGET GTest::gmock APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(GTest::gmock PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libgmock.so.1.14.0"
  IMPORTED_SONAME_RELEASE "libgmock.so.1.14.0"
  )

list(APPEND _cmake_import_check_targets GTest::gmock )
list(APPEND _cmake_import_check_files_for_GTest::gmock "${_IMPORT_PREFIX}/lib/libgmock.so.1.14.0" )

# Import target "GTest::gmock_main" for configuration "Release"
set_property(TARGET GTest::gmock_main APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(GTest::gmock_main PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libgmock_main.so.1.14.0"
  IMPORTED_SONAME_RELEASE "libgmock_main.so.1.14.0"
  )

list(APPEND _cmake_import_check_targets GTest::gmock_main )
list(APPEND _cmake_import_check_files_for_GTest::gmock_main "${_IMPORT_PREFIX}/lib/libgmock_main.so.1.14.0" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
