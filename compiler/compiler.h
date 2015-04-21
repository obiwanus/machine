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


struct Node
{
    enum {
        CLASS, CLASS_VAR_DEC, TYPE, SUBROUTINE_DEC, PARAMETER_LIST,
        SUBROUTINE_BODY, VAR_DEC, CLASS_NAME, SUBROUTINE_NAME, VAR_NAME,
        STATEMENTS, STATEMENT, LET_STATEMENT, IF_STATEMENT, WHILE_STATEMENT,
        DO_STATEMENT, RETURN_STATEMENT, EXPRESSION, TERM, SUBROUTINE_CALL,
        EXPRESSION_LIST, OP, UNARY_OP, KEYWORD_CONSTANT
    } type;
    char name[MAXLINE];
    int child_count;
    struct Node **children;
};

typedef struct Node Node;


#endif  // COMPILER_H
