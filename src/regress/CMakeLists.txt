include_directories(../libc4/include ${APR_INCLUDES})
link_directories(../libc4)

add_executable(driver driver.c)
target_link_libraries(driver c4)
if(APU_LDFLAGS)
    set_target_properties(driver PROPERTIES LINK_FLAGS ${APU_LDFLAGS})
endif(APU_LDFLAGS)
