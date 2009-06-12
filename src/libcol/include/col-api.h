#ifndef COL_API_H
#define COL_API_H

typedef struct ColInstance ColInstance;

ColInstance *col_init();
void col_shutdown(ColInstance *col);

#endif  /* COL_API_H */
