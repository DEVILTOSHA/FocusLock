# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Release")
  file(REMOVE_RECURSE
  "CMakeFiles\\FocusLock_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\FocusLock_autogen.dir\\ParseCache.txt"
  "FocusLock_autogen"
  )
endif()
