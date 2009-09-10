cmake_minimum_required(VERSION 2.6)
project(Col)
subdirs(src)

set(PACKAGE_VERSION 1)
set(BUILD_SHARED_LIBS on)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

find_package(Apr REQUIRED)
find_package(Apu REQUIRED)
find_package(BerkeleyDB)
find_package(Bison REQUIRED)
find_package(Flex REQUIRED)

include_directories(${APR_INCLUDES} ${APU_INCLUDES})
link_libraries(${APR_LIBS} ${APR_EXTRALIBS} ${APU_LIBS} ${APU_EXTRALIBS})

set(CMAKE_C_FLAGS "-g -Wall -W -Wmissing-prototypes -pedantic -std=gnu99 ${APR_CFLAGS} ${CMAKE_C_FLAGS}")
message (STATUS "CFLAGS: ${CMAKE_C_FLAGS}")
