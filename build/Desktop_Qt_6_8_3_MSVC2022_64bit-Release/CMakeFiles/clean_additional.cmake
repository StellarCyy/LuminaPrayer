# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Release")
  file(REMOVE_RECURSE
  "CMakeFiles\\LuminaPrayer_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\LuminaPrayer_autogen.dir\\ParseCache.txt"
  "LuminaPrayer_autogen"
  )
endif()
