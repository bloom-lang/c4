include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

file(GLOB_RECURSE libcol_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/*.c)

FLEX_TARGET(Lexer ${CMAKE_CURRENT_SOURCE_DIR}/parser/scan.l ${PROJECT_BINARY_DIR}/scan.c)
BISON_TARGET(Parser ${CMAKE_CURRENT_SOURCE_DIR}/parser/parse.y ${PROJECT_BINARY_DIR}/parse.c)
ADD_FLEX_BISON_DEPENDENCY(Lexer Parser)

SET(libcol_SRCS ${libcol_SRCS} ${FLEX_Lexer_OUTPUTS})
SET(libcol_SRCS ${libcol_SRCS} ${BISON_Parser_OUTPUTS})

add_library(col ${libcol_SRCS})
