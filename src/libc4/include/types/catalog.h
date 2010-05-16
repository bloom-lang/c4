#ifndef CATALOG_H
#define CATALOG_H

#include "c4-api-callback.h"
#include "parser/ast.h"
#include "types/datum.h"
#include "types/schema.h"
#include "util/list.h"

typedef struct C4Catalog C4Catalog;

struct AbstractTable;
struct OpChainList;
struct Tuple;

typedef struct CallbackRecord CallbackRecord;

/*
 * A TableDef is a container for metadata about a C4 table. Because this
 * struct is shared among threads, fields of the structure should not be
 * modified.
 */
typedef struct TableDef
{
    apr_pool_t *pool;
    char *name;
    AstStorageKind storage;
    Schema *schema;

    /* Column number of location spec, or -1 if none */
    int ls_colno;

    /* List of callbacks registered for this table */
    CallbackRecord *cb;

    /* Table implementation */
    struct AbstractTable *table;

    /*
     * We keep a direct pointer to the router's list of op chains that have
     * this table as their delta table. This is mildly gross from a layering
     * POV, but useful for perf: it means we can skip a hash table lookup
     * for each derived tuple that is routed.
     */
    struct OpChainList *op_chain_list;
} TableDef;

struct CallbackRecord
{
    C4TupleCallback callback;
    void *data;
    struct CallbackRecord *next;
};

C4Catalog *cat_make(C4Runtime *c4);

void cat_define_table(C4Catalog *cat, const char *name,
                      AstStorageKind storage, List *schema);
void cat_delete_table(C4Catalog *cat, const char *name);
bool cat_table_exists(C4Catalog *cat, const char *name);
TableDef *cat_get_table(C4Catalog *cat, const char *name);
struct AbstractTable *cat_get_table_impl(C4Catalog *cat, const char *name);

void cat_register_callback(C4Catalog *cat, const char *tbl_name,
                           C4TupleCallback callback, void *data);
void table_invoke_callbacks(struct Tuple *tuple, TableDef *tbl, bool is_delete);

bool is_numeric_type(DataType type_id);
bool is_valid_type_name(const char *type_name);
DataType get_type_id(const char *type_name);
char *get_type_name(DataType type_id);

datum_hash_func type_get_hash_func(DataType type);
datum_eq_func type_get_eq_func(DataType type);
datum_cmp_func type_get_cmp_func(DataType type);
datum_bin_in_func type_get_binary_in_func(DataType type);
datum_text_in_func type_get_text_in_func(DataType type);
datum_bin_out_func type_get_binary_out_func(DataType type);
datum_text_out_func type_get_text_out_func(DataType type);

#endif  /* CATALOG_H */
