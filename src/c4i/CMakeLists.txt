include_directories(../libc4/include)
link_directories(../libc4)

add_executable(coverlog cli.c)
target_link_libraries(coverlog c4)
if(APU_LDFLAGS)
    set_target_properties(coverlog PROPERTIES LINK_FLAGS ${APU_LDFLAGS})
endif(APU_LDFLAGS)
