#ifndef PARSER_H
#define PARSER_H

/* from scan.l */
extern int	yylex(void);
extern void yyerror(const char *message);

/* from gram.y */
extern int	yyparse(void);

#endif  /* PARSER_H */
