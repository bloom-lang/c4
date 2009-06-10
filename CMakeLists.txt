cmake_minimum_required(VERSION 2.6)
project(Col)
subdirs(src)

set(package_versin 1)
set(build_shared_libs on)

SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
find_package(flex)
find_package(bison)

SET(CMAKE_C_FLAGS "-g -Wall -W -pedantic -std=gnu99 ${CMAKE_C_FLAGS}")
