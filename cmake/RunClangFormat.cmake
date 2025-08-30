# Runs clang-format either in 'check' mode (-n -Werror) or 'format' mode (-i)
# on all source files under src/ and tests/.
#
# Inputs (via -D VAR=...):
#   - CLANG_FORMAT_EXE: Path to clang-format executable (required)
#   - SOURCE_DIR: Root of the repository (defaults to script parent dir/..)
#   - MODE: 'check' (default) or 'format'

if(NOT DEFINED CLANG_FORMAT_EXE)
  message(FATAL_ERROR "CLANG_FORMAT_EXE not set")
endif()

if(NOT DEFINED SOURCE_DIR)
  # Assume this script lives in <repo>/cmake, so SOURCE_DIR is one up
  get_filename_component(_SCRIPT_DIR "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
  get_filename_component(SOURCE_DIR "${_SCRIPT_DIR}/.." ABSOLUTE)
endif()

if(NOT DEFINED MODE)
  set(MODE "check")
endif()

set(_args)
if(MODE STREQUAL "check")
  list(APPEND _args -n -Werror)
elseif(MODE STREQUAL "format")
  list(APPEND _args -i)
else()
  message(FATAL_ERROR "Unknown MODE='${MODE}' (expected 'check' or 'format')")
endif()

file(GLOB_RECURSE FILES_TO_PROCESS
  RELATIVE "${SOURCE_DIR}"
  "${SOURCE_DIR}/src/*.c" "${SOURCE_DIR}/src/*.h"
  "${SOURCE_DIR}/tests/*.c" "${SOURCE_DIR}/tests/*.h"
)

if(FILES_TO_PROCESS STREQUAL "")
  message(STATUS "No source files found for clang-format under ${SOURCE_DIR}/src and ${SOURCE_DIR}/tests")
  return()
endif()

set(_fail_count 0)
foreach(_rel IN LISTS FILES_TO_PROCESS)
  set(_abs "${SOURCE_DIR}/${_rel}")
  execute_process(
    COMMAND "${CLANG_FORMAT_EXE}" ${_args} "${_abs}"
    RESULT_VARIABLE _res
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE _err
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE
  )
  if(NOT _res EQUAL 0)
    math(EXPR _fail_count "${_fail_count} + 1")
    message(STATUS "clang-format ${MODE} failed: ${_rel}\n${_out}\n${_err}")
  endif()
endforeach()

if(_fail_count GREATER 0)
  message(FATAL_ERROR "clang-format ${MODE} failed on ${_fail_count} file(s)")
else()
  list(LENGTH FILES_TO_PROCESS _total)
  message(STATUS "clang-format ${MODE} passed on ${_total} file(s)")
endif()
