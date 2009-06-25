#ifndef CATALOG_H
#define CATALOG_H

#include "types/schema.h"

typedef struct ColCatalog ColCatalog;

ColCatalog *cat_make(ColInstance *col);

Schema *cat_get_schema(ColCatalog *cat, const char *name);
void cat_set_schema(ColCatalog *cat, const char *name, Schema *schema);

#endif  /* CATALOG_H */
