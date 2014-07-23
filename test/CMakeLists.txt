set(test_SOURCES main.cc
    cputime.cc unittest.cc buffertest.cc coreutilstest.cc coretest.cc)
set(test_HEADERS
    cputime.hh unittest.hh buffertest.hh coreutilstest.hh coretest.hh)

add_executable(sdr_test ${test_SOURCES})
target_link_libraries(sdr_test ${LIBS} libsdr)
