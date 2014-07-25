IF(SDR_WITH_QT5 AND SDR_WITH_FFTW AND SDR_WITH_PORTAUDIO)
 add_executable(sdr_spec sdr_spec.cc)
 target_link_libraries(sdr_spec ${LIBS} ${QT_LIBRARIES} libsdr libsdr-gui )
ENDIF(SDR_WITH_QT5 AND SDR_WITH_FFTW AND SDR_WITH_PORTAUDIO)

IF(SDR_WITH_PORTAUDIO)
 add_executable(sdr_wavplay sdr_wavplay.cc)
 target_link_libraries(sdr_wavplay ${LIBS} libsdr)
ENDIF(SDR_WITH_PORTAUDIO)

IF(SDR_WITH_QT5 AND SDR_WITH_FFTW AND SDR_WITH_PORTAUDIO)
 add_executable(sdr_rds sdr_rds.cc)
 target_link_libraries(sdr_rds ${LIBS} ${QT_LIBRARIES} libsdr libsdr-gui)
ENDIF(SDR_WITH_QT5 AND SDR_WITH_FFTW AND SDR_WITH_PORTAUDIO)

add_executable(sdr_psk31 sdr_psk31.cc)
target_link_libraries(sdr_psk31 ${LIBS} ${QT_LIBRARIES} libsdr libsdr-gui)
