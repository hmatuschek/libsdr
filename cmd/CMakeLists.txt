#add_subdirectory(libhttpd)

add_executable(sdr_cmd main.cc)
target_link_libraries(sdr_cmd ${LIBS} libsdr)
