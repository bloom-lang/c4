#ifndef PARSER_H
#define PARSER_H

typedef struct ColParser ColParser;

ColParser *parser_init(ColInstance *col);
ColStatus parser_destroy(ColParser *parser);

#endif  /* PARSER_H */
