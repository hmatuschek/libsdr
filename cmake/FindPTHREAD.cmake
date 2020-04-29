#---
# File: FindPTHREAD.cmake
#
# Find the native pthread-win32 includes and libraries.
#
# This module defines:
#
# PTHREAD_INCLUDE_DIR, where to find pthread.h, etc.
# PTHREAD_LIBRARY, libraries to link against to use pthread.
# PTHREAD_FOUND, True if found, false if one of the above are not found.
#
#---

find_path(PTHREAD_INCLUDE_DIR pthread.h PATHS
          ${CMAKE_INSTALL_PREFIX}/include
          $ENV{PTHREAD_DIR}/include
          ${PTHREAD_DIR}/include
          C:/pthread-win32/include
          C:/pthread-win64/include
          ${PTHREAD_PACKAGE_INCLUDE_HINT}
)

foreach(EXHAND C CE SE)
	foreach(COMPAT 1 2 3)
		list(APPEND PTHREAD_W32_LIBRARY "pthreadV${EXHAND}${COMPAT}")
	endforeach()
endforeach()
find_library(PTHREAD_LIBRARY NAMES ${PTHREAD_W32_LIBRARY} pthread pthread_dll pthread_lib PATHS
             ${CMAKE_INSTALL_PREFIX}/lib
             ${CMAKE_INSTALL_PREFIX}/lib64
             $ENV{PTHREAD_DIR}/lib
             $ENV{PTHREAD_DIR}/lib64
             ${PTHREAD_DIR}/lib
             ${PTHREAD_DIR}/lib64
             C:/pthread-win32/lib
             C:/pthread-win64/lib
             ${PTHREAD_PACKAGE_INCLUDE_HINT}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PTHREAD DEFAULT_MSG
                                  PTHREAD_LIBRARY
                                  PTHREAD_INCLUDE_DIR)

add_library(PTHREAD::PTHREAD INTERFACE IMPORTED)
