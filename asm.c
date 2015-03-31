#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAXLINE 1000

typedef enum {
    A_COMMAND = 1, 
    C_COMMAND, 
    L_COMMAND
} command_type;

// A parsed command
typedef struct
{
    command_type type;
    int error;  // if any
    
    // For A or L commands
    char var[MAXLINE];
    char label[MAXLINE];

    // For C command
    char comp[10];
    char dest[10];
    char jump[10];

    // A cleaned command for debugging
    char cln[MAXLINE]; 
} Command;

// A symbol table entry
typedef struct
{
    char symbol[MAXLINE];
    long value;
} Entry;

// A symbol table
typedef struct
{
    unsigned int len;
    unsigned int allocated;
    Entry *entries;
} SymbolTable;

int symbol_exists(SymbolTable *table, char *symbol)
{
    if (!table->len)
    {
        return -1;
    }

    for (int i = 0; i < table->len; i++)
    {
        if (strcmp(symbol, table->entries[i].symbol) == 0)
            return i;
    }

    return -1;
}

void symbol_push(SymbolTable *table, char *symbol, long value)
{
    if (table->entries == NULL)
    {
        // Allocate memory
        table->allocated = 100;
        table->entries = malloc(table->allocated * sizeof(Entry));
    }
    if (table->len >= (table->allocated - 2))
    {
        // Allocate more
        table->allocated = table->allocated * 2;
        table->entries = realloc(table->entries, table->allocated * sizeof(Entry));
    }
    
    int pos = symbol_exists(table, symbol);
    if (pos == -1)
    {
        pos = table->len;  // the next free position
        strcpy(table->entries[pos].symbol, symbol);
        table->len++;
    }
    table->entries[pos].value = value;
}

long symbol_get(SymbolTable *table, char *symbol)
{
    int pos = symbol_exists(table, symbol);
    if (pos == -1)
        return -1;
    return table->entries[pos].value;
}


void load_special_vars(SymbolTable *table)
{
    symbol_push(table, "R0", 0);
    symbol_push(table, "R1", 1);
    symbol_push(table, "R2", 2);
    symbol_push(table, "R3", 3);
    symbol_push(table, "R4", 4);
    symbol_push(table, "R5", 5);
    symbol_push(table, "R6", 6);
    symbol_push(table, "R7", 7);
    symbol_push(table, "R8", 8);
    symbol_push(table, "R9", 9);
    symbol_push(table, "R10", 10);
    symbol_push(table, "R11", 11);
    symbol_push(table, "R12", 12);
    symbol_push(table, "R13", 13);
    symbol_push(table, "R14", 14);
    symbol_push(table, "R15", 15);

    symbol_push(table, "SP", 0);
    symbol_push(table, "LCL", 1);
    symbol_push(table, "ARG", 2);
    symbol_push(table, "THIS", 3);
    symbol_push(table, "THAT", 4);

    symbol_push(table, "SCREEN", 16384);
    symbol_push(table, "KBD", 24576);
}

void reverse(char s[])
{
    int length = strlen(s) ;
    int c, i, j;

    for (i = 0, j = length - 1; i < j; i++, j--) {
        c = s[i];
        s[i] = s[j];
        s[j] = c;
    }
}

void itob(int n, char s[], int minlen)
{
    int i, digit;

    i = 0;
    do {
        digit = n % 2;
        s[i++] = digit + '0';
    } while ((n /= 2) != 0);

    while (i < minlen)
        s[i++] = '0';
    s[i] = '\0';
    reverse(s);
}


int string_in_array(char *array[], char *string, int len)
{
    if (strcmp(string, "") == 0)
        return 0;

    for (int i = 0; i < len; i++)
    {
        if (strcmp(array[i], string) == 0)
            return i;
    }
    return -1;
}


int parse_line(char *source_line, Command *command)
{
    char line[MAXLINE];
    char *cln = line;
    int len = 0;

    // Cleaning
    source_line[strcspn(source_line, "\r\n")] = '\0';  // trim 
    while (*source_line != 0) {
        if (*source_line == ' ') 
        {
            source_line++;
            continue;
        }
        else if (*source_line == '/' && *(source_line + 1) == '/') {
            break;  // don't copy the rest
        }
        *cln++ = *source_line++;
    }
    *cln = '\0';
    
    if ((len = strlen(line)) == 0)
        return 0;

    strcpy(command->cln, line);

    // Parsing
    if (line[0] == '@')
    {
        sscanf(line, "@%s", command->var);
        command->type = A_COMMAND;
        return len;
    }
    if (line[0] == '(' && line[len-1] == ')')
    {
        sscanf(line, "(%s", command->label);    
        command->label[len-2] = '\0';  // trim the closing bracket
        command->type = L_COMMAND;
        if (!strlen(command->label))
            command->error = 1;
        return len;
    }
    char *eq = strchr(line, '=');
    if (eq != NULL)
    {
        strncpy(command->dest, line, eq - line);
        strcpy(command->comp, eq + 1);
        if (!strlen(command->dest) || !strlen(command->comp))
            command->error = 1;
        command->type = C_COMMAND;
        return len;
    }
    char *semicolon = strchr(line, ';');
    if (semicolon != NULL)
    {
        strncpy(command->comp, line, semicolon - line);
        strcpy(command->jump, semicolon + 1);
        if (!strlen(command->jump) || !strlen(command->comp))
            command->error = 1;
        command->type = C_COMMAND;
        return len;
    }
    
    // Unknown command
    command->error = 1;
    return len;
}


int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("No source file specified.\n");
        return 1;
    }

    char *filename = argv[1];
    FILE *source = fopen(filename, "rb");
    if (source == NULL) 
    {
        printf("Cannot open file %s\n", filename);   
        return 1; 
    }

    SymbolTable symbol_table = {};
    load_special_vars(&symbol_table);
    
    char line[MAXLINE];
    Command command = {};
    unsigned int line_num = 0;  // source line num
    unsigned int ml_line = 0;   // machine language line num

    while (fgets(line, MAXLINE, source))
    {
        command = (Command){0};  // clear command
        line_num++;

        if (!parse_line(line, &command))
            continue;
        
        if (command.error > 0)
        {
            printf("Syntax error at line %d\n", line_num);
            exit(1);
        }

        if (command.type == A_COMMAND || command.type == C_COMMAND)
        {
            ml_line++;
        }
        else if (command.type == L_COMMAND)
        {
            // Push symbol into the symbol table
            if (symbol_exists(&symbol_table, command.label) > 0)
            {
                printf("Value error: (%s) is repeated at line %d\n", command.label, line_num);
                exit(1);
            }
            symbol_push(&symbol_table, command.label, ml_line);
            // printf("{%s}={%d}={%d}\n", command.label, ml_line, symbol_get(&symbol_table, command.label));
        }
    }

    // Rewind
    fseek(source, 0, SEEK_SET);

    // Generate the code
    char *code = malloc(17 * (line_num + 1) * sizeof(char));  // 16 'bits' + \n
    char code_line[MAXLINE];
    line_num = 0;
    int free_mem = 16;
    char *COMPS[] = {"0", "1", "-1", "D",  "A", "!D", "!A", "-D", "-A", "D+1", "A+1",
                     "D-1", "A-1", "D+A", "A+D", "D-A", "A-D", "D&A", "A&D", "D|A", "A|D",
                     "M", "!M", "-M", "M+1", "M-1", "D+M", "M+D", "D-M", "M-D", 
                     "D&M", "M&D", "D|M", "M|D"};
    char *COMP_VALS[] = {"0101010", "0111111", "0111010", "0001100", "0110000",
                         "0001101", "0110001", "0001111", "0110011", "0011111",
                         "0110111", "0001110", "0110010", "0000010", "0000010", 
                         "0010011", "0000111", "0000000", "0000000", "0010101",
                         "0010101",
                         "1110000", "1110001", "1110011", "1110111", "1110010",
                         "1000010", "1000010", "1010011", "1000111", "1000000",
                         "1000000", "1010101", "1010101"};
    char *DESTS[] = {"null", "M", "D", "MD", "DM", "A", "AM", "MA", 
                     "AD", "DA", "AMD", "MDA", "MAD", "DMA", "ADM", "DAM"};
    char *DEST_VALS[] = {"000", "001", "010", "011", "011", "100", "101", "101", 
                         "110", "110", "111", "111", "111", "111", "111", "111"};
    char *JUMPS[] = {"null", "JGT", "JEQ", "JGE", "JLT", "JNE", "JLE", "JMP"};
    char *JUMP_VALS[] = {"000", "001", "010", "011", "100", "101", "110", "111"};

    // for (int i = 0; i < symbol_table.len; i++)
    // {
    //     printf("%s\n", symbol_table.entries[i].symbol);
    // }
    // printf("total: %d\n", symbol_table.len);

    while (fgets(line, MAXLINE, source))
    {
        command = (Command){0};  // clear command
        line_num++;

        if (!parse_line(line, &command))
            continue;
        if (command.type != A_COMMAND && command.type != C_COMMAND)
            continue;  // we generate code only for those two

        if (command.type == A_COMMAND)
        {
            int number = atoi(command.var);
            if (number == 0 && isalpha(command.var[0]))
            {
                number = symbol_get(&symbol_table, command.var);
                if (number == -1)
                {
                    number = free_mem++;
                    symbol_push(&symbol_table, command.var, number);
                }
            }
            char binary[20];
            itob(number, binary, 15);
            sprintf(code_line, "0%s\n", binary);
        }
        else if (command.type == C_COMMAND)
        {
            int comp = string_in_array(COMPS, command.comp, 34);
            int dest = string_in_array(DESTS, command.dest, 16);
            int jump = string_in_array(JUMPS, command.jump, 8);
            if (comp == -1 || dest == -1 || jump == -1)
            {
                printf("Value error: Unknown command '%s' at line %d\n", command.cln, line_num);
                exit(1);
            }
            sprintf(code_line, "111%s%s%s\n", COMP_VALS[comp], DEST_VALS[dest], JUMP_VALS[jump]);
        }
        strcat(code, code_line);
        *code_line = 0;
    }

    char dest_filename[MAXLINE];
    strcpy(dest_filename, filename);
    strcat(dest_filename, ".hack");
    FILE *dest = fopen(dest_filename, "wb");

    fputs(code, dest);  // finally

    fclose(source);
    fclose(dest);

    return 0;
}
