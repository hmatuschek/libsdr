# Sources of libsdr
set(LIBSDR_SOURCES
    buffer.cc node.cc queue.cc traits.cc
    portaudio.cc utils.cc wavfile.cc
    exception.cc logger.cc psk31.cc options.cc)
set(LIBSDR_HEADERS sdr.hh math.hh
    buffer.hh node.hh queue.hh buffernode.hh filternode.hh traits.hh autocast.hh
    siggen.hh portaudio.hh utils.hh wavfile.hh demod.hh firfilter.hh
    fftplan.hh fftplan_native.hh exception.hh baseband.hh freqshift.hh subsample.hh
    combine.hh logger.hh psk31.hh interpolate.hh operators.hh options.hh)

if(SDR_WITH_PORTAUDIO)
  set(LIBSDR_SOURCES ${LIBSDR_SOURCES} portaudio.cc)
  set(LIBSDR_HEADERS ${LIBSDR_HEADERS} portaudio.hh)
endif(SDR_WITH_PORTAUDIO)

if(SDR_WITH_FFTW)
  set(LIBSDR_SOURCES ${LIBSDR_SOURCES})
  set(LIBSDR_HEADERS ${LIBSDR_HEADERS} fftplan_fftw3.hh)
endif(SDR_WITH_FFTW)

if(SDR_WITH_RTLSDR)
  set(LIBSDR_SOURCES ${LIBSDR_SOURCES} rtlsource.cc)
  set(LIBSDR_HEADERS ${LIBSDR_HEADERS} rtlsource.hh)
endif(SDR_WITH_RTLSDR)

add_custom_target(libsdr_hdrs SOURCES ${LIBSDR_HEADERS})

add_library(libsdr SHARED ${LIBSDR_SOURCES})
set_target_properties(libsdr PROPERTIES OUTPUT_NAME sdr)
set_target_properties(libsdr PROPERTIES DEPENDS libsdr_hdrs)
set_target_properties(libsdr PROPERTIES VERSION
  "${libsdr_VERSION_MAJOR}.${libsdr_VERSION_MINOR}.${libsdr_VERSION_PATCH}")
set_target_properties(libsdr PROPERTIES SOVERION "${libsdr_VERSION_MAJOR}")
set_target_properties(libsdr PROPERTIES MACOSX_RPATH "${CMAKE_INSTALL_RPATH}")
set_target_properties(libsdr PROPERTIES INSTALL_NAME_DIR ${CMAKE_INSTALL_FULL_LIBDIR})
target_link_libraries(libsdr ${LIBS})

install(TARGETS libsdr DESTINATION ${CMAKE_INSTALL_LIBDIR})
INSTALL_HEADERS_WITH_DIRECTORY("${LIBSDR_HEADERS}" "${CMAKE_INSTALL_INCLUDEDIR}/libsdr")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/config.hh"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/libsdr")

