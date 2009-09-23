#ifndef CATALOG_H
#define CATALOG_H

#include "types/datum.h"
#include "types/schema.h"
#include "util/list.h"

typedef struct ColCatalog ColCatalog;

struct ColTable;
struct OpChainList;

/*
 * A TableDef is a container for metadata about a COL table. Because this
 * struct is shared among threads, fields of the structure should not be
 * modified.
 */
typedef struct TableDef
{
    apr_pool_t *pool;
    char *name;
    Schema *schema;
    List *key_list;

    /* Column number of location spec, or -1 if none */
    int ls_colno;

    /* Table implementation */
    struct ColTable *table;

    /*
     * We keep a direct pointer to the router's list of op chains that have
     * this table as their delta table. This is mildly gross from a layering
     * POV, but useful for perf: it means we can skip a hash table lookup
     * for each derived tuple that is routed.
     */
    struct OpChainList *op_chain_list;
} TableDef;

ColCatalog *cat_make(ColInstance *col);

void cat_define_table(ColCatalog *cat, const char *name,
                      List *schema, List *key_list);
void cat_delete_table(ColCatalog *cat, const char *name);
TableDef *cat_get_table(ColCatalog *cat, const char *name);
struct ColTable *cat_get_table_impl(ColCatalog *cat, const char *name);

Schema *cat_get_schema(ColCatalog *cat, const char *name);

bool is_numeric_type(DataType type_id);
bool is_valid_type_name(const char *type_name);
DataType get_type_id(const char *type_name);
char *get_type_name(DataType type_id);

datum_hash_func type_get_hash_func(DataType type);
datum_eq_func type_get_eq_func(DataType type);
datum_bin_in_func type_get_binary_in_func(DataType type);
datum_text_in_func type_get_text_in_func(DataType type);
datum_bin_out_func type_get_binary_out_func(DataType type);
datum_text_out_func type_get_text_out_func(DataType type);

#endif  /* CATALOG_H */
