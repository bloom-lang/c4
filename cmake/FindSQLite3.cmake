# Find the SQLite3 include files and libraries. Sets the following
# variables:
#   SQLITE3_INCLUDE_DIR
#   SQLITE3_FOUND
#   SQLITE3_LIBRARIES

find_path(SQLITE3_INCLUDE_DIR sqlite3.h
    /opt/local/include
    /usr/local/include
    /usr/include
)

find_library(SQLITE3_LIBRARY NAMES sqlite3)

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(SQLITE3 DEFAULT_MSG SQLITE3_LIBRARY SQLITE3_INCLUDE_DIR)

if(SQLITE3_FOUND)
  set(SQLITE3_LIBRARIES ${SQLITE3_LIBRARY})
endif(SQLITE3_FOUND)
