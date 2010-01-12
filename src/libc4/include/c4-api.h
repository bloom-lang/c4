#ifndef C4_API_H
#define C4_API_H

#include "c4-api-callback.h"

/*
 * An opaque type that holds the client-side state associated with a
 * single instance of the C4 runtime. C4 API calls use this state to
 * communicate with an instance of the C4 runtime, which runs
 * asynchronously as a separate thread.
 */
typedef struct C4Client C4Client;

/*
 * The return status of a C4 API call. C4_OK is "success"; any other
 * value means that a failure occurred.
 */
typedef enum C4Status
{
    C4_OK = 0,
    C4_ERROR
} C4Status;

/*
 * Initialize and terminate the C4 library for the current process. Must be the
 * first and last API functions called, respectively.
 */
void c4_initialize(void);
void c4_terminate(void);

C4Client *c4_make(int port);
C4Status c4_destroy(C4Client *c4);
int c4_get_port(C4Client *c4);

C4Status c4_install_file(C4Client *c4, const char *path);
C4Status c4_install_str(C4Client *c4, const char *str);

char *c4_dump_table(C4Client *c4, const char *tbl_name);

/*
 * Register a callback that is invoked for each tuple inserted into the
 * specified table. The "data" argument is passed into the callback.
 *
 * In the current implementation, callbacks are called synchronously (blocking
 * the runtime until the callback returns), and are invoked immediately after a
 * table insertion occurs (i.e. not at fixpoint boundaries).
 */
C4Status c4_register_callback(C4Client *c4, const char *tbl_name,
                              C4TupleCallback callback, void *data);

#endif  /* C4_API_H */
