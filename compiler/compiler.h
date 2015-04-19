#ifndef COMPILER_H
#define COMPILER_H

#define COUNT_OF(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

#define MAXLINE 1000


typedef struct
{
    enum {
        ILLEGAL, KEYWORD, SYMBOL,
        IDENTIFIER, INT_CONST, STRING_CONST
    } type;
    char repr[MAXLINE];
} Token;


typedef struct
{
    // Required to be cleared to zero
    int len;
    int allocated;
    Token *entries;
} Tokens;


typedef struct
{
    enum {
        PARSE_BLANK,  // required to be first
        PARSE_ERROR,
        PARSE_SUCCESS
    } code;
    char message[200];
} Parse_result;


#endif  // COMPILER_H
