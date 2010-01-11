include_directories(../libc4/include ${APR_INCLUDES})
link_directories(../libc4)

add_executable(bench bench.c)
target_link_libraries(bench c4)
if(APU_LDFLAGS)
    set_target_properties(bench PROPERTIES LINK_FLAGS ${APU_LDFLAGS})
endif(APU_LDFLAGS)
