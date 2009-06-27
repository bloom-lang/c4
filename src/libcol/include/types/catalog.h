#ifndef CATALOG_H
#define CATALOG_H

#include "types/schema.h"

typedef struct ColCatalog ColCatalog;

ColCatalog *cat_make(ColInstance *col);

Schema *cat_get_schema(ColCatalog *cat, const char *name);
void cat_set_schema(ColCatalog *cat, const char *name, Schema *schema);

bool is_numeric_type(DataType type_id);
bool is_valid_type_name(const char *type_name);
DataType get_type_id(const char *type_name);
char *get_type_name(DataType type_id);

#endif  /* CATALOG_H */
