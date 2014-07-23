# - Find FFTW
# Find the native FFTW includes and library
#
#  FFTWSingle_INCLUDES    - where to find fftw3.h
#  FFTWSingle_LIBRARIES   - List of libraries when using FFTW.
#  FFTWSingle_FOUND       - True if FFTW found.

if (FFTWSingle_INCLUDES)
  # Already in cache, be silent
  set (FFTWSingle_FIND_QUIETLY TRUE)
endif (FFTWSingle_INCLUDES)

find_path (FFTWSingle_INCLUDES fftw3.h)

find_library (FFTWSingle_LIBRARIES NAMES fftw3f)

# handle the QUIETLY and REQUIRED arguments and set FFTW_FOUND to TRUE if
# all listed variables are TRUE
include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (FFTWSingle DEFAULT_MSG FFTWSingle_LIBRARIES FFTWSingle_INCLUDES)

mark_as_advanced (FFTWSingle_LIBRARIES FFTWSingle_INCLUDES)
