#include <apr_hash.h>

#include "c4-internal.h"
#include "parser/ast.h"
#include "router.h"
#include "types/catalog.h"
#include "types/tuple.h"
#include "storage/table.h"

struct C4Catalog
{
    C4Runtime *c4;
    apr_pool_t *pool;

    /* A map from table names => TableDef */
    apr_hash_t *tbl_def_tbl;
};

C4Catalog *
cat_make(C4Runtime *c4)
{
    apr_pool_t *pool;
    C4Catalog *cat;

    pool = make_subpool(c4->pool);
    cat = apr_pcalloc(pool, sizeof(*cat));
    cat->c4 = c4;
    cat->pool = pool;
    cat->tbl_def_tbl = apr_hash_make(cat->pool);

    return cat;
}

/*
 * Return the column number of the loc spec, or -1 if the schema has no
 * location specifier.
 */
static int
find_loc_spec_colno(List *schema)
{
    int i;
    ListCell *lc;

    i = 0;
    foreach (lc, schema)
    {
        AstSchemaElt *elt = (AstSchemaElt *) lc_ptr(lc);

        if (elt->is_loc_spec)
            return i;

        i++;
    }

    return -1;
}

void
cat_define_table(C4Catalog *cat, const char *name, AstStorageKind storage,
                 List *schema, List *key_list)
{
    apr_pool_t *tbl_pool;
    TableDef *tbl_def;

    if (cat_get_table(cat, name) != NULL)
        ERROR("Duplicate table definition: %s", name);

    tbl_pool = make_subpool(cat->pool);
    tbl_def = apr_pcalloc(tbl_pool, sizeof(*tbl_def));
    tbl_def->pool = tbl_pool;
    tbl_def->name = apr_pstrdup(tbl_pool, name);
    tbl_def->storage = storage;
    tbl_def->schema = schema_make_from_ast(schema, tbl_pool);
    tbl_def->key_list = list_copy(key_list, tbl_pool);
    tbl_def->ls_colno = find_loc_spec_colno(schema);
    tbl_def->cb = NULL;
    tbl_def->table = table_make(tbl_def, cat->c4, tbl_pool);
    tbl_def->op_chain_list = router_get_opchain_list(cat->c4->router,
                                                     tbl_def->name);

    apr_hash_set(cat->tbl_def_tbl, tbl_def->name,
                 APR_HASH_KEY_STRING, tbl_def);
}

void
cat_delete_table(C4Catalog *cat, const char *name)
{
    TableDef *tbl_def;

    tbl_def = cat_get_table(cat, name);
    if (tbl_def == NULL)
        ERROR("No such table: %s", name);

    apr_hash_set(cat->tbl_def_tbl, name, APR_HASH_KEY_STRING, NULL);
    apr_pool_destroy(tbl_def->pool);
}

TableDef *
cat_get_table(C4Catalog *cat, const char *name)
{
    return (TableDef *) apr_hash_get(cat->tbl_def_tbl, name,
                                     APR_HASH_KEY_STRING);
}

AbstractTable *
cat_get_table_impl(C4Catalog *cat, const char *name)
{
    return (cat_get_table(cat, name))->table;
}

Schema *
cat_get_schema(C4Catalog *cat, const char *name)
{
    TableDef *tbl_def;

    tbl_def = cat_get_table(cat, name);
    if (!tbl_def)
        return NULL;

    return tbl_def->schema;
}

void
cat_register_callback(C4Catalog *cat, const char *tbl_name,
                      C4TableCallback callback, void *data)
{
    TableDef *tbl_def;
    CallbackRecord *cb_rec;

    tbl_def = cat_get_table(cat, tbl_name);
    if (tbl_def == NULL)
        ERROR("No such table: %s", tbl_name);

    cb_rec = apr_palloc(cat->pool, sizeof(*cb_rec));
    cb_rec->callback = callback;
    cb_rec->data = data;
    cb_rec->next = tbl_def->cb;
    tbl_def->cb = cb_rec;
}

void
table_invoke_callbacks(TableDef *tbl_def, Tuple *tuple)
{
    CallbackRecord *cb_rec = tbl_def->cb;

    while (cb_rec != NULL)
    {
        cb_rec->callback(tuple, tbl_def, cb_rec->data);
        cb_rec = cb_rec->next;
    }
}

bool
is_numeric_type(DataType type_id)
{
    switch (type_id)
    {
        case TYPE_DOUBLE:
        case TYPE_INT2:
        case TYPE_INT4:
        case TYPE_INT8:
            return true;

        default:
            return false;
    }
}

bool
is_valid_type_name(const char *type_name)
{
    return (bool) (get_type_id(type_name) != TYPE_INVALID);
}

DataType
get_type_id(const char *type_name)
{
    if (strcmp(type_name, "bool") == 0)
        return TYPE_BOOL;
    if (strcmp(type_name, "char") == 0)
        return TYPE_CHAR;
    if (strcmp(type_name, "double") == 0)
        return TYPE_DOUBLE;
    if (strcmp(type_name, "int") == 0)
        return TYPE_INT4;
    if (strcmp(type_name, "int2") == 0)
        return TYPE_INT2;
    if (strcmp(type_name, "int4") == 0)
        return TYPE_INT4;
    if (strcmp(type_name, "int8") == 0)
        return TYPE_INT8;
    if (strcmp(type_name, "string") == 0)
        return TYPE_STRING;

    return TYPE_INVALID;
}

char *
get_type_name(DataType type_id)
{
    switch (type_id)
    {
        case TYPE_INVALID:
            return "invalid";
        case TYPE_BOOL:
            return "bool";
        case TYPE_CHAR:
            return "char";
        case TYPE_DOUBLE:
            return "double";
        case TYPE_INT2:
            return "int2";
        case TYPE_INT4:
            return "int4";
        case TYPE_INT8:
            return "int8";
        case TYPE_STRING:
            return "string";

        default:
            ERROR("Unexpected type id: %d", type_id);
    }
}

datum_hash_func
type_get_hash_func(DataType type)
{
    switch (type)
    {
        case TYPE_BOOL:
            return bool_hash;

        case TYPE_CHAR:
            return char_hash;

        case TYPE_DOUBLE:
            return double_hash;

        case TYPE_INT2:
            return int2_hash;

        case TYPE_INT4:
            return int4_hash;

        case TYPE_INT8:
            return int8_hash;

        case TYPE_STRING:
            return string_hash;

        case TYPE_INVALID:
            ERROR("Invalid data type: TYPE_INVALID");

        default:
            ERROR("Unexpected data type: %uc", type);
    }
}

datum_eq_func
type_get_eq_func(DataType type)
{
    switch (type)
    {
        case TYPE_BOOL:
            return bool_equal;

        case TYPE_CHAR:
            return char_equal;

        case TYPE_DOUBLE:
            return double_equal;

        case TYPE_INT2:
            return int2_equal;

        case TYPE_INT4:
            return int4_equal;

        case TYPE_INT8:
            return int8_equal;

        case TYPE_STRING:
            return string_equal;

        case TYPE_INVALID:
            ERROR("Invalid data type: TYPE_INVALID");

        default:
            ERROR("Unexpected data type: %uc", type);
    }
}

datum_bin_in_func
type_get_binary_in_func(DataType type)
{
    switch (type)
    {
        case TYPE_BOOL:
            return bool_from_buf;

        case TYPE_CHAR:
            return char_from_buf;

        case TYPE_DOUBLE:
            return double_from_buf;

        case TYPE_INT2:
            return int2_from_buf;

        case TYPE_INT4:
            return int4_from_buf;

        case TYPE_INT8:
            return int8_from_buf;

        case TYPE_STRING:
            return string_from_buf;

        case TYPE_INVALID:
            ERROR("Invalid data type: TYPE_INVALID");

        default:
            ERROR("Unexpected data type: %uc", type);
    }
}

datum_text_in_func
type_get_text_in_func(DataType type)
{
    switch (type)
    {
        case TYPE_BOOL:
            return bool_from_str;

        case TYPE_CHAR:
            return char_from_str;

        case TYPE_DOUBLE:
            return double_from_str;

        case TYPE_INT2:
            return int2_from_str;

        case TYPE_INT4:
            return int4_from_str;

        case TYPE_INT8:
            return int8_from_str;

        case TYPE_STRING:
            return string_from_str;

        case TYPE_INVALID:
            ERROR("Invalid data type: TYPE_INVALID");

        default:
            ERROR("Unexpected data type: %uc", type);
    }
}

/*
 * Returns a function pointer that converts a datum from the in-memory
 * format into the binary (network) format.
 */
datum_bin_out_func
type_get_binary_out_func(DataType type)
{
    switch (type)
    {
        case TYPE_BOOL:
            return bool_to_buf;

        case TYPE_CHAR:
            return char_to_buf;

        case TYPE_DOUBLE:
            return double_to_buf;

        case TYPE_INT2:
            return int2_to_buf;

        case TYPE_INT4:
            return int4_to_buf;

        case TYPE_INT8:
            return int8_to_buf;

        case TYPE_STRING:
            return string_to_buf;

        case TYPE_INVALID:
            ERROR("Invalid data type: TYPE_INVALID");

        default:
            ERROR("Unexpected data type: %uc", type);
    }
}

datum_text_out_func
type_get_text_out_func(DataType type)
{
    switch (type)
    {
        case TYPE_BOOL:
            return bool_to_str;

        case TYPE_CHAR:
            return char_to_str;

        case TYPE_DOUBLE:
            return double_to_str;

        case TYPE_INT2:
            return int2_to_str;

        case TYPE_INT4:
            return int4_to_str;

        case TYPE_INT8:
            return int8_to_str;

        case TYPE_STRING:
            return string_to_str;

        case TYPE_INVALID:
            ERROR("Invalid data type: TYPE_INVALID");

        default:
            ERROR("Unexpected data type: %uc", type);
    }
}
