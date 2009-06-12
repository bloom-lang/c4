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

/*
 * Initialize and terminate COL for the current process. Must be the first
 * and last API function called, respectively.
 */
void col_initialize();
void col_terminate();

/* Create and destroy individual instances of COL. */
ColInstance *col_make();
ColStatus col_destroy(ColInstance *col);

ColStatus col_install_file(ColInstance *col, const char *path);
ColStatus col_install_str(ColInstance *col, const char *str);

ColStatus col_start(ColInstance *col);

#endif  /* COL_API_H */
