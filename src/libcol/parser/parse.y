#include "util/list.h"

%union
{
    char *str;
    const char *keyword;
    List *list;
}

%token <keyword> PROGRAM DEFINE
%start input

%type <str>     program_header
%type <list>    program_body

%%
input: program_header program_body {}

program_header: PROGRAM identifier { $$ = $1; }

program_body: {}
%%
