include_directories(../libc4/include ${APR_INCLUDES})
link_directories(../libc4)

add_executable(c4i c4i.c)
target_link_libraries(c4i c4)
if(APU_LDFLAGS)
    set_target_properties(c4i PROPERTIES LINK_FLAGS ${APU_LDFLAGS})
endif(APU_LDFLAGS)
