include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

file(GLOB_RECURSE libcol_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/*.c)

FLEX_TARGET(MyLexer parser/scan.l ${PROJECT_BINARY_DIR}/scan.l)
BISON_TARGET(MyParser parser/parse.y ${PROJECT_BINARY_DIR}/parse.c)
#ADD_FLEX_BISON_DEPENDENCY(MyLexer MyParser)

SET(libcol_SRCS ${libcol_SRCS} ${FLEX_MyLexer_OUTPUTS})
SET(libcol_SRCS ${libcol_SRCS} ${BISON_MyParser_OUTPUTS})

add_library(col ${libcol_SRCS})
