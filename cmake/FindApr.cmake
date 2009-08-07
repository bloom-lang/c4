# Source: http://svn.trolocsis.com/repos/projects/templates/apr/build/FindAPR.cmake
# Locate APR include paths and libraries

# This module defines
# APR_INCLUDES, where to find apr.h, etc.
# APR_LIBS, linker switches to use with ld to link against apr
# APR_EXTRALIBS, additional libraries to link against
# APR_CFLAGS, the flags to use to compile
# APR_FOUND, set to 'yes' if found

find_program(APR_CONFIG_EXECUTABLE apr-1-config)
mark_as_advanced(APR_CONFIG_EXECUTABLE)

macro(_apr_invoke _varname _regexp)
    execute_process(
        COMMAND ${APR_CONFIG_EXECUTABLE} ${ARGN}
        OUTPUT_VARIABLE _apr_output
        RESULT_VARIABLE _apr_failed
    )

    if(_apr_failed)
        message(FATAL_ERROR "apr-1-config ${ARGN} failed")
    else(_apr_failed)
        string(REGEX REPLACE "[\r\n]"  "" _apr_output "${_apr_output}")
        string(REGEX REPLACE " +$"     "" _apr_output "${_apr_output}")

        if(NOT ${_regexp} STREQUAL "")
            string(REGEX REPLACE "${_regexp}" " " _apr_output "${_apr_output}")
        endif(NOT ${_regexp} STREQUAL "")

        # XXX: We don't want to invoke separate_arguments() for APR_CFLAGS;
        # just leave as-is
        if(NOT ${_varname} STREQUAL "APR_CFLAGS")
            separate_arguments(_apr_output)
        endif(NOT ${_varname} STREQUAL "APR_CFLAGS")

        set(${_varname} "${_apr_output}")
    endif(_apr_failed)
endmacro(_apr_invoke)

_apr_invoke(APR_INCLUDES  "(^| )-I" --includes)
_apr_invoke(APR_CFLAGS     ""        --cppflags --cflags)
_apr_invoke(APR_EXTRALIBS "(^| )-l" --libs)
_apr_invoke(APR_LIBS      ""        --link-ld)

if(APR_INCLUDES AND APR_EXTRALIBS AND APR_LIBS AND APR_CFLAGS)
    set(APR_FOUND "YES")
    message (STATUS "apr found: YES ${APR_LIBS}")
endif(APR_INCLUDES AND APR_EXTRALIBS AND APR_LIBS AND APR_CFLAGS)
