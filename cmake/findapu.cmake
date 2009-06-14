# Locate apr-util include paths and libraries. Based on findapr.cmake;
# simple modifications to apply to apr-util instead.

# This module defines
# APU_INCLUDES, where to find apu.h, etc.
# APU_LIBS, the libraries to link against to use apr-util
# APU_DEFINITIONS, definitions to use when compiling code that uses apr-util
# APU_FOUND, set to 'yes' if found

find_program(APU_CONFIG_EXECUTABLE apu-1-config)
mark_as_advanced(APU_CONFIG_EXECUTABLE)

macro(_apu_invoke _varname _regexp)
    execute_process(
        COMMAND ${APU_CONFIG_EXECUTABLE} ${ARGN}
        OUTPUT_VARIABLE _apu_output
        RESULT_VARIABLE _apu_failed
    )

    if(_apu_failed)
        message(FATAL_ERROR "apu-1-config ${ARGN} failed")
    else(_apu_failed)
        string(REGEX REPLACE "[\r\n]"  "" _apu_output "${_apu_output}")
        string(REGEX REPLACE " +$"     "" _apu_output "${_apu_output}")

        if(NOT ${_regexp} STREQUAL "")
            string(REGEX REPLACE "${_regexp}" " " _apu_output "${_apu_output}")
        endif(NOT ${_regexp} STREQUAL "")

        separate_arguments(_apu_output)
        set(${_varname} "${_apu_output}")
    endif(_apu_failed)
endmacro(_apu_invoke)

_apu_invoke(APU_INCLUDES  "(^| )-I" --includes)
_apu_invoke(APU_EXTRALIBS "(^| )-l" --libs)
_apu_invoke(APU_LIBS      ""        --link-ld)

if(APU_INCLUDES AND APU_EXTRALIBS AND APU_LIBS)
    set(APU_FOUND "YES")
    message (STATUS "apu found: YES ${APU_LIBS}")
endif(APU_INCLUDES AND APU_EXTRALIBS AND APU_LIBS)
