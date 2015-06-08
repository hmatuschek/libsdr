#
# Implements the packing of resource files into a header file
#
get_filename_component(OUTPUT_FILE ${OUTPUT} NAME)
message(STATUS "Generate resource file '${OUTPUT_FILE}' from: ${FILES}")
# Create empty file
file(WRITE ${OUTPUT} "")
# For each resource file
foreach(file ${FILES})
  # Normalize filename
  string(REGEX MATCH "([^/]+)$" filename ${file})
  string(REGEX REPLACE "\\.| " "_" filename ${filename})
  # Read and convert file content
  file(READ "${RESOURCE_PATH}/${file}" filedata HEX)
  string(REGEX REPLACE "([0-9a-fA-F][0-9a-fA-F])" "0x\\1," filedata ${filedata})
  # Update output file
  file(APPEND ${OUTPUT}
       "extern \"C\" {\n"
       "  static char ${filename}[] = {${filedata}};\n"
       "  static unsigned ${filename}_size = sizeof(${filename});\n"
       "}\n")
endforeach()
