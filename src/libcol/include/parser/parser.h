#ifndef PARSER_H
#define PARSER_H

#include "parser/ast.h"

typedef struct ColParser ColParser;

ColParser *parser_init(ColInstance *col);
ColStatus parser_destroy(ColParser *parser);

AstProgram *parser_do_parse(ColParser *parser, const char *str);

#endif  /* PARSER_H */
