#ifndef COMPILER_H
#define COMPILER_H

#define COUNT_OF(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

#define MAXLINE 1000


typedef struct
{
    enum {
        PARSE_BLANK,  // required to be first
        PARSE_ERROR,
        PARSE_SUCCESS
    } code;
    char message[200];
    Command *command;
} Parse_result;


typedef struct
{
    enum {
        KEYWORD, SYMBOL, IDENTIFIER, INT_CONST, STRING_CONST
    } type;
    char value[MAXLINE];
} Token;


typedef struct
{
    // Required to be cleared to zero
    int len;
    int allocated;
    Token *entries;
} Tokens;


#endif  // COMPILER_H
