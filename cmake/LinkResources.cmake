macro(link_resources HEADER_NAME)
  #create_resources("${CMAKE_CURRENT_BINARY_DIR}/${HEADER_NAME}.hh" ${ARGN})
  add_custom_target(${HEADER_NAME} ALL
   COMMAND ${CMAKE_COMMAND} -DOUTPUT="${CMAKE_CURRENT_BINARY_DIR}/${HEADER_NAME}.hh" -DRESOURCE_PATH="${CMAKE_CURRENT_SOURCE_DIR}" -DFILES="${ARGN}" -P "${PROJECT_SOURCE_DIR}/cmake/create_resources.cmake"
   DEPENDS ${ARGN} SOURCES ${ARGN})
endmacro(link_resources)

