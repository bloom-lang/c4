#ifndef CATALOG_H
#define CATALOG_H

#include "types/schema.h"

typedef struct ColCatalog ColCatalog;

typedef struct TableDef
{
    apr_pool_t *pool;
    char *name;
    Schema *schema;
    List *key_list;
} TableDef;

ColCatalog *cat_make(ColInstance *col);

void cat_define_table(ColCatalog *cat, const char *name,
                      List *type_list, List *key_list);
void cat_delete_table(ColCatalog *cat, const char *name);
TableDef *cat_get_table(ColCatalog *cat, const char *name);

Schema *cat_get_schema(ColCatalog *cat, const char *name);
void cat_set_schema(ColCatalog *cat, const char *name, Schema *schema);

bool is_numeric_type(DataType type_id);
bool is_valid_type_name(const char *type_name);
DataType get_type_id(const char *type_name);
char *get_type_name(DataType type_id);

#endif  /* CATALOG_H */
