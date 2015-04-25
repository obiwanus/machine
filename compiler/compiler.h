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
} Parse_result;


typedef enum
{
    ILLEGAL, KEYWORD, SYMBOL,
    IDENTIFIER, INT_CONST, STRING_CONST
} token_type;


typedef struct
{
    token_type type;
    char repr[MAXLINE];
    char *filename;
    char line_num;
} Token;


typedef struct
{
    // Required to be cleared to zero
    int len;
    int allocated;
    Token *entries;
    Token *next;
} Tokens;





#endif  // COMPILER_H
