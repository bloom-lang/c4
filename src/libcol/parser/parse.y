%union
 {
     char *str;
     const char *keyword;
}

%token <keyword> PROGRAM DEFINE
