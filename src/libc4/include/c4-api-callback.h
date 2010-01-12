#ifndef C4_API_CALLBACK_H
#define C4_API_CALLBACK_H

/*
 * Declarations related to callbacks that are part of the C4 client API. These
 * are located in a separate header, so that the runtime can get access to the
 * callback definitions without including the rest of the client API.
 */
struct TableDef;
struct Tuple;

typedef void (*C4TableCallback)(struct Tuple *tuple,
                                struct TableDef *tbl_def, void *data);

#endif  /* C4_API_CALLBACK_H */
