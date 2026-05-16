# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Release")
  file(REMOVE_RECURSE
  "CMakeFiles\\PDC_Monitor_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\PDC_Monitor_autogen.dir\\ParseCache.txt"
  "PDC_Monitor_autogen"
  )
endif()
