#ifndef VM_H
#define VM_H

#define COUNT_OF(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

#define MAXLINE 1000

typedef enum {
    C_NONE = 0, C_ARITHMETIC, C_CMP, C_UNARY, C_PUSH, C_POP, 
    C_LABEL, C_GOTO, C_IF, C_FUNCTION, C_RETURN, C_CALL
} command_type;

typedef struct
{
    command_type type;
    char name[20];
    char arg1[20];
    char arg2[20];
    char cln[MAXLINE];  // cleaned source line
} Command;

typedef struct
{
    // Required to be cleared to zero
    int len;
    int allocated;
    Command *entries;
} ParsedCommands;

typedef enum {
    PARSE_BLANK,  // required to be first
    PARSE_ERROR,
    PARSE_SUCCESS
} parse_result_code;

typedef struct
{
    parse_result_code code;
    char message[200];
    Command *command;
} parse_result;


#endif  // VM_H
