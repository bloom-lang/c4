include_directories(../libcol/include)
link_directories(../libcol)

add_executable(coverlog cli.c)
target_link_libraries(coverlog col)
set_target_properties(coverlog PROPERTIES LINK_FLAGS ${APU_LDFLAGS})
