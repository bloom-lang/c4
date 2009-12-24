include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include ${PROJECT_BINARY_DIR})

file(GLOB_RECURSE libc4_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/*.c)

flex_target(Lexer ${CMAKE_CURRENT_SOURCE_DIR}/parser/ol_scan.l ${PROJECT_BINARY_DIR}/ol_scan.c COMPILE_FLAGS "--header-file=${PROJECT_BINARY_DIR}/ol_scan.h")
bison_target(Parser ${CMAKE_CURRENT_SOURCE_DIR}/parser/ol_parse.y ${PROJECT_BINARY_DIR}/ol_parse.c COMPILE_FLAGS "-d")
add_flex_bison_dependency(Lexer Parser)

set(libc4_SRCS ${libc4_SRCS} ${FLEX_Lexer_OUTPUTS} ${BISON_Parser_OUTPUTS})

add_library(c4 ${libc4_SRCS})
target_link_libraries(c4 sqlite3)
target_link_libraries(c4 ${APR_LIBS} ${APR_EXTRALIBS} ${APU_LIBS} ${APU_EXTRALIBS})

if(APU_LDFLAGS)
    set_target_properties(c4 PROPERTIES LINK_FLAGS ${APU_LDFLAGS})
endif(APU_LDFLAGS)
