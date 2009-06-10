include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

file(GLOB_RECURSE libcol_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/*.c)

add_library(col ${libcol_SRCS})

