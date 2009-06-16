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
 * and last API functions called, respectively.
 */
void col_initialize(void);
void col_terminate(void);

/* Create and destroy individual instances of COL. */
ColInstance *col_make(int port);
ColStatus col_destroy(ColInstance *col);

ColStatus col_install_file(ColInstance *col, const char *path);
ColStatus col_install_str(ColInstance *col, const char *str);

ColStatus col_start(ColInstance *col);

/* Test API */
void col_set_other(ColInstance *col, int target_port);
void col_do_ping(ColInstance *col);

#endif  /* COL_API_H */
