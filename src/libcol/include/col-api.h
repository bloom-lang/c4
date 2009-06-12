#ifndef COL_API_H
#define COL_API_H

/*
 * An opaque type that holds the state associated with a single instance of
 * the COL runtime.
 */
typedef struct ColInstance ColInstance;

/*
 * The return status of a COL API call. COL_OK is "success"; any other value
 * means that a failure occurred.
 */
typedef enum ColStatus
{
    COL_OK = 0,
    COL_ERROR
} ColStatus;

ColInstance *col_init();
ColStatus col_shutdown(ColInstance *col);

ColStatus col_install_file(const char *path);
ColStatus col_install_str(const char *str);

#endif  /* COL_API_H */
