include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include ${PROJECT_BINARY_DIR})

file(GLOB_RECURSE libcol_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/*.c)

FLEX_TARGET(Lexer ${CMAKE_CURRENT_SOURCE_DIR}/parser/ol_scan.l ${PROJECT_BINARY_DIR}/ol_scan.c COMPILE_FLAGS "--header-file=${PROJECT_BINARY_DIR}/ol_scan.h")
BISON_TARGET(Parser ${CMAKE_CURRENT_SOURCE_DIR}/parser/ol_parse.y ${PROJECT_BINARY_DIR}/ol_parse.c COMPILE_FLAGS "-d")
ADD_FLEX_BISON_DEPENDENCY(Lexer Parser)

SET(libcol_SRCS ${libcol_SRCS} ${FLEX_Lexer_OUTPUTS})
SET(libcol_SRCS ${libcol_SRCS} ${BISON_Parser_OUTPUTS})

add_library(col ${libcol_SRCS})
