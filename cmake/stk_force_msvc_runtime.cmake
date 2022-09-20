macro(force_msvc_runtime)
  message(STATUS "force_msvc_runtime_called")
  if(MSVC)
    message(STATUS "MSVC defined")
    if(NOT "${FORCE_MSVC_RUNTIME}" STREQUAL "")
      message(STATUS "Calling replace loop.")
      # Set compiler options.
      message(STATUS "MSVC: Forcing runtime to ${FORCE_MSVC_RUNTIME}.")
      foreach(variable CMAKE_C_FLAGS_DEBUG
          CMAKE_C_FLAGS_MINSIZEREL
          CMAKE_C_FLAGS_RELEASE
          CMAKE_C_FLAGS_RELWITHDEBINFO
          CMAKE_CXX_FLAGS_DEBUG
          CMAKE_CXX_FLAGS_MINSIZEREL
          CMAKE_CXX_FLAGS_RELEASE
          CMAKE_CXX_FLAGS_RELWITHDEBINFO)
        message(STATUS "Checking ${variable}")
        #if(variable MATCHES "/M[TD]d?")
            message(STATUS "Replacing ${${variable}}")
            string(REGEX REPLACE "/M[TD]d?" "${FORCE_MSVC_RUNTIME}" ${variable} "${${variable}}")
            message(STATUS "with      ${${variable}}")
        #endif()
      endforeach()
    endif()
  endif()
endmacro()

macro(force_msvc_debug_define)
  message(STATUS "force_msvc_debug_define")
  if(MSVC)
    message(STATUS "MSVC defined")
    if(NOT "${FORCE_MSVC_DEBUG_DEFINE}" STREQUAL "")
      message(STATUS "Calling replace loop.")
      # Set compiler options.
      message(STATUS "MSVC: Forcing MSVC debug define to ${FORCE_MSVC_DEBUG_DEFINE}.")
      foreach(variable CMAKE_C_FLAGS_DEBUG
          CMAKE_C_FLAGS_MINSIZEREL
          CMAKE_C_FLAGS_RELEASE
          CMAKE_C_FLAGS_RELWITHDEBINFO
          CMAKE_CXX_FLAGS_DEBUG
          CMAKE_CXX_FLAGS_MINSIZEREL
          CMAKE_CXX_FLAGS_RELEASE
          CMAKE_CXX_FLAGS_RELWITHDEBINFO)
        message(STATUS "Checking ${variable}")
        message(STATUS "Replacing ${${variable}}")
        string(REGEX REPLACE "/D[N]?DEBUG?" "${FORCE_MSVC_DEBUG_DEFINE}" ${variable} "${${variable}}")
        message(STATUS "with      ${${variable}}")
      endforeach()
    endif()
  endif()
endmacro()
