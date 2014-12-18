IF(SDR_WITH_QT5 AND SDR_WITH_FFTW AND SDR_WITH_PORTAUDIO)
 add_executable(sdr_spec sdr_spec.cc)
 target_link_libraries(sdr_spec ${LIBS} ${QT_LIBRARIES} libsdr libsdr-gui )
ENDIF(SDR_WITH_QT5 AND SDR_WITH_FFTW AND SDR_WITH_PORTAUDIO)

IF(SDR_WITH_PORTAUDIO)
 add_executable(sdr_wavplay sdr_wavplay.cc)
 target_link_libraries(sdr_wavplay ${LIBS} libsdr)

 add_executable(sdr_fm sdr_fm.cc)
 target_link_libraries(sdr_fm ${LIBS} libsdr)

 add_executable(sdr_rec sdr_rec.cc)
 target_link_libraries(sdr_rec ${LIBS} libsdr)

 add_executable(sdr_afsk1200 sdr_afsk1200.cc)
 target_link_libraries(sdr_afsk1200 ${LIBS} libsdr)

 qt5_wrap_cpp(sdr_schumann_MOC_SOURCES schumann.hh)
 add_executable(sdr_schumann sdr_schumann.cc schumann.cc ${sdr_schumann_MOC_SOURCES})
 target_link_libraries(sdr_schumann ${LIBS} libsdr libsdr-gui)
ENDIF(SDR_WITH_PORTAUDIO)

IF(SDR_WITH_QT5 AND SDR_WITH_FFTW AND SDR_WITH_PORTAUDIO)
 add_executable(sdr_rds sdr_rds.cc)
 target_link_libraries(sdr_rds ${LIBS} ${QT_LIBRARIES} libsdr libsdr-gui)
ENDIF(SDR_WITH_QT5 AND SDR_WITH_FFTW AND SDR_WITH_PORTAUDIO)

add_executable(sdr_psk31 sdr_psk31.cc)
target_link_libraries(sdr_psk31 ${LIBS} ${QT_LIBRARIES} libsdr libsdr-gui)


