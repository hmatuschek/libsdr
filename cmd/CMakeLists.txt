
# Creates a header file including the shared resources
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})
link_resources(sdr_cmd_resources shared/index.html)

add_executable(sdr_cmd main.cc)
add_dependencies(sdr_cmd sdr_cmd_resources)
target_link_libraries(sdr_cmd ${LIBS} libsdr )
