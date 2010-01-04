#ifndef C4_API_H
#define C4_API_H

/*
 * An opaque type that holds the state associated with a single
 * instance of the C4 runtime.
 */
typedef struct C4Instance C4Instance;

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
 * Initialize and terminate C4 for the current process. Must be the
 * first and last API functions called, respectively.
 */
void c4_initialize(void);
void c4_terminate(void);

/* Create and destroy individual instances of C4. */
C4Instance *c4_make(int port);
C4Status c4_destroy(C4Instance *c4);

C4Status c4_install_file(C4Instance *c4, const char *path);
C4Status c4_install_str(C4Instance *c4, const char *str);

#endif  /* C4_API_H */
