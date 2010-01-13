#include "c4-internal.h"
#include "types/catalog.h"
#include "operator/scancursor.h"
#include "storage/table.h"
#include "util/dump_table.h"

void
dump_table(C4Runtime *c4, const char *tbl_name, StrBuf *buf)
{
    AbstractTable *table;
    ScanCursor *cursor;
    Tuple *scan_tuple;

    table = cat_get_table_impl(c4->cat, tbl_name);

    cursor = table->scan_make(table, c4->tmp_pool);
    table->scan_reset(table, cursor);
    while ((scan_tuple = table->scan_next(table, cursor)) != NULL)
    {
        tuple_to_str_buf(scan_tuple, buf);
        sbuf_append_char(buf, '\n');
    }

    sbuf_append_char(buf, '\0');
}
