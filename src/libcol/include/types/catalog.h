#ifndef CATALOG_H
#define CATALOG_H

#include "types/schema.h"

typedef struct ColCatalog ColCatalog;

ColCatalog *cat_make(ColInstance *col);

Schema *cat_get_schema(ColCatalog *cat, const char *name);
void cat_set_schema(ColCatalog *cat, const char *name, Schema *schema);

bool is_valid_type_name(const char *type_name);
DataType lookup_type_name(const char *type_name);

#endif  /* CATALOG_H */
