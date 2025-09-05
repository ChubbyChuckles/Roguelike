# Path utilities for cross-platform normalization
# Usage:
#   rogue_normalize_path(out_var "C:\\path\\to\\thing")
# Produces forward-slash CMake-style paths regardless of host platform.

function(rogue_normalize_path out_var in_path)
    if(NOT DEFINED in_path OR "${in_path}" STREQUAL "")
        set(${out_var} "" PARENT_SCOPE)
        return()
    endif()
    file(TO_CMAKE_PATH "${in_path}" _norm)
    set(${out_var} "${_norm}" PARENT_SCOPE)
endfunction()
