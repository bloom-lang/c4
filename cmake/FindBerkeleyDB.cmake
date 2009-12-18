# Find the Berkeley DB include files and libraries. Sets the following
# variables:
#   BERKELEY_DB_INCLUDE_DIR
#   BERKELEY_DB_FOUND
#   BERKELEY_DB_LIBRARIES

find_path(BERKELEY_DB_INCLUDE_DIR db.h
    /opt/local/include/db47
    /usr/local/include/db4
    /usr/local/include
    /usr/include/db4
    /usr/include
)

find_library(BERKELEY_DB_LIBRARY NAMES db)

if (BERKELEY_DB_LIBRARY AND BERKELEY_DB_INCLUDE_DIR)
  set(BERKELEY_DB_LIBRARIES ${BDB_LIBRARY})
  set(BERKELEY_DB_FOUND "YES")
else ()
  set(BERKELEY_DB_FOUND "NO")
endif ()


if (BERKELEY_DB_FOUND)
  if (NOT BERKELEY_DB_FIND_QUIETLY)
    message(STATUS "Found BerkeleyDB: ${BDB_LIBRARIES}")
  else ()
    message(STATUS "NOT FOUND BDB")
  endif ()
else ()
  if (BERKELEY_DB_FIND_REQUIRED)
    message(FATAL_ERROR "Could not find BerkeleyDB library")
  endif ()
endif ()
