macro(force_msvc_runtime)
  message(STATUS "force_msvc_runtime_called")
  if(MSVC)
    if(NOT "${FORCE_MSVC_RUNTIME}" STREQUAL "")
      # Set compiler options.
      set(variables
          CMAKE_C_FLAGS_DEBUG
          CMAKE_C_FLAGS_MINSIZEREL
          CMAKE_C_FLAGS_RELEASE
          CMAKE_C_FLAGS_RELWITHDEBINFO
          CMAKE_CXX_FLAGS_DEBUG
          CMAKE_CXX_FLAGS_MINSIZEREL
          CMAKE_CXX_FLAGS_RELEASE
          CMAKE_CXX_FLAGS_RELWITHDEBINFO)
      message(STATUS "MSVC: Forcing runtime to ${FORCE_MSVC_RUNTIME}.")
      foreach(variable ${variables})
        if(${variable} matches "^/M[TD]d?$" regex)
            string(regex replace "^/M[TD]d?$" "${FORCE_MSVC_RUNTIME}" ${variable} "${${variable}}")
        endif()
      endforeach()
    endif()
  endif()
endmacro()
