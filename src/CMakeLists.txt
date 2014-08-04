# Sources of libsdr
set(LIBSDR_SOURCES
    buffer.cc node.cc queue.cc traits.cc
    portaudio.cc utils.cc wavfile.cc
    exception.cc logger.cc psk31.cc)
set(LIBSDR_HEADERS sdr.hh math.hh
    buffer.hh node.hh queue.hh buffernode.hh filternode.hh traits.hh autocast.hh
    siggen.hh portaudio.hh utils.hh wavfile.hh demod.hh firfilter.hh
    fftplan.hh fftplan_native.hh exception.hh baseband.hh freqshift.hh subsample.hh
    combine.hh logger.hh psk31.hh interpolate.hh)

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
target_link_libraries(libsdr ${LIBS})

if(SDR_WITH_FFTW AND SDR_WITH_QT5)
  set(libsdr_gui_SOURCES gui/spectrum.cc gui/spectrumview.cc gui/waterfallview.cc)
  set(libsdr_gui_MOC_HEADERS gui/spectrum.hh gui/spectrumview.hh gui/waterfallview.hh)
  set(libsdr_gui_HEADERS ${libsdr_gui_MOC_HEADERS} gui/gui.hh)
  qt5_wrap_cpp(libsdr_gui_MOC_SOURCES ${libsdr_gui_MOC_HEADERS})

  add_library(libsdr-gui SHARED ${libsdr_gui_SOURCES} ${libsdr_gui_MOC_SOURCES})
  set_target_properties(libsdr-gui PROPERTIES OUTPUT_NAME sdr-gui)
  target_link_libraries(libsdr-gui libsdr ${Qt5Core_LIBRARIES} ${Qt5Widgets_LIBRARIES})
endif(SDR_WITH_FFTW AND SDR_WITH_QT5)

