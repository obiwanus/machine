#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vm.h"


parse_result parse_line(char *line, ParsedCommands *commands);
void encode(char *line,  Command *command, int line_num, char *filename);


int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("No source file specified\n");
        return 1;
    }

    char *filename = argv[1];
    FILE *source = fopen(filename, "rb");
    if (source == NULL) 
    {
        printf("Cannot open file %s\n", filename);   
        return 1; 
    }

    ParsedCommands commands = {};
    int line_num = 0;
    char line[MAXLINE];
    parse_result result;

    while (fgets(line, MAXLINE, source))
    {
        line_num++;

        result = parse_line(line, &commands);
        if (result.code == PARSE_BLANK)
            continue;
        if (result.code == PARSE_ERROR)
        {
            printf("Syntax error at line %d: \n%s\n", line_num, line);
            printf("%s\n", result.message);
            exit(1);
        }
    }

    fclose(source);

    // Write file
    char *extension = strrchr(filename, '.');
    if (extension && !strcmp(extension, ".vm"))
        *extension = 0;  // cut the extension
    char dest_filename[MAXLINE];
    strcpy(dest_filename, filename);
    strcat(dest_filename, ".asm");
    FILE *dest = fopen(dest_filename, "wb");
    char code_line[MAXLINE];
    
    // Get filename for unique labels
    char enc_fn[MAXLINE];  
    strcpy(enc_fn, filename);
    int i = 0;
    while (enc_fn[i] != 0 && i < MAXLINE - 1)
    {
        if (enc_fn[i] == '.' || enc_fn[i] == '/')
            enc_fn[i] = '$';
        i++;
    }

    // Encode lines
    for (int i = 0; i < commands.len; i++)
    {
        encode(code_line, &commands.entries[i], i, enc_fn);
        fputs(code_line, dest);
    }

    fclose(dest);
    printf("File %s written\n", dest_filename);

    return 0;
}

void clean_line(char *line)
{
    // Deletes multiple spaces and trims
    // whitespace on both sides

    char *cln = line, *beginning = line;
    int space_num = 0;
    int non_space_num = 0;

    while (*line != 0)     
    {
        if (*line == ' ' && (!non_space_num || space_num > 1)) 
        {
            line++;
            space_num++;
            continue;
        }
        else if (*line == '/' && *(line + 1) == '/') 
        {
            break;  // don't copy the rest
        }
        else if (*line == '\r' || *line == '\n')
        {
            break;  // line ends
        }
        if (*line == ' ') 
        {
            space_num++;
        }
        else
        {
            non_space_num++;
            space_num = 0;
        }
        *cln++ = *line++;
    }

    // Trim whitespace on the right
    while (*(cln - 1) == ' ' && cln > beginning)
        cln--;

    *cln = 0;
}

int in_array(char *string, char *array[], int len)
{
    // Returns the index of first occurrence
    // of string in the array or -1

    for (int i = 0; i < len; i++)
    {
        if (strcmp(array[i], string) == 0)
            return i;
    }
    return -1;
}

Command *new_command(ParsedCommands *list)
{
    // Returns the pointer to the next free slot
    // for a parsed command. Allocates space.

    Command *result = 0;

    if (list->entries == 0)
    {
        list->allocated = 100;
        list->entries = malloc(list->allocated * sizeof(Command));
    }
    if (list->len >= (list->allocated - 2))
    {
        list->allocated = list->allocated * 2;
        list->entries = realloc(list->entries, 
                                    list->allocated * sizeof(Command));
    }
    result = list->entries + list->len;
    list->len++;

    return result;
}

parse_result_code validate_arguments(Command *command, parse_result *result)
{
    if (command->type == C_PUSH || command->type == C_POP)
    {
        char *SEGMENTS[] = {
            "argument", "local", "static", "constant",
            "this", "that", "pointer", "temp"
        };
        if (in_array(command->arg1, SEGMENTS, COUNT_OF(SEGMENTS)) == -1)
        {
            strcpy(result->message, "Unknown argument");
            return PARSE_ERROR;
        }
        if (atoi(command->arg2) == 0 && strcmp(command->arg2, "0") != 0)
        {
            strcpy(result->message, "Non-integer segment index");
            return PARSE_ERROR;
        }
        if (strcmp(command->arg1, "pointer") == 0)
        {
            if (strcmp(command->arg2, "0") != 0 &&
                strcmp(command->arg2, "1") != 0)
            {
                strcpy(result->message, "pointer index must be either 0 or 1");
                return PARSE_ERROR;
            }
        }
        if (strcmp(command->arg1, "temp") == 0)
        {
            int index = atoi(command->arg2);
            if (index > 7 || index < 0)
            {
                strcpy(result->message, "temp index must be within range 0 - 7");
                return PARSE_ERROR;
            }
        }
    }
    else if (command->type == C_ARITHMETIC ||
             command->type == C_CMP ||
             command->type == C_UNARY) 
    {
        if (strcmp(command->arg1, "") != 0 ||
            strcmp(command->arg2, "") != 0)
        {
            strcpy(result->message, "Unexpected argument");
            return PARSE_ERROR;
        }
    }
    else if (command->type == C_LABEL ||
             command->type == C_GOTO ||
             command->type == C_IF_GOTO)
    {
        if (strcmp(command->arg1, "") == 0)
        {
            strcpy(result->message, "Argument required");
            return PARSE_ERROR;
        }
        if (strcmp(command->arg2, "") != 0)
        {
            strcpy(result->message, "Unexpected argument");
            return PARSE_ERROR;
        }
    }

    return PARSE_SUCCESS;
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


void itoa(int n, char s[], int base)
{
    // Convert integer to string
    int i, digit;

    i = 0;
    do {
        digit = n % base;
        s[i++] = digit + '0';
    } while ((n /= base) != 0);

    s[i] = '\0';
    reverse(s);
}


parse_result parse_line(char *line, ParsedCommands *commands)
{
    parse_result result = {};

    clean_line(line);

    if (strlen(line) == 0)
        return result;  // PARSE_BLANK

    Command *command = new_command(commands);
    result.command = command;

    strcpy(command->cln, line);
    
    char *NAMES[] = {
        "eq", "gt", "lt", "add", "sub", "and", "or", "not", "neg",
        "push", "pop", "label", "goto", "if-goto"  // to be extended
    };
    int TYPES[] = { C_PUSH, C_POP, C_LABEL, C_GOTO, C_IF_GOTO };

    sscanf(line, "%s %s %s", command->name, command->arg1, command->arg2);

    int found_pos = in_array(command->name, NAMES, COUNT_OF(NAMES));
    if (found_pos == -1)
    {
        result.code = PARSE_ERROR;
        strcpy(result.message, "Unknown command");
        return result;
    }
    if (found_pos <= 2)
        command->type = C_CMP;
    else if (found_pos <= 6)
        command->type = C_ARITHMETIC;
    else if (found_pos <= 8)
        command->type = C_UNARY;
    else
        command->type = TYPES[found_pos - 9];
    
    result.code = validate_arguments(command, &result);
    return result;         
}

void encode(char *line,  Command *command, int line_num, char *filename)
{
    *line = 0;
    
    char diff[20];
    char *NAMES[] = {"eq", "gt", "lt", "add", "sub", "and", "or", "not", "neg"};
    char *DIFFS[] = {
        "D;JEQ", "D;JGT", "D;JLT",
        "M=M+D", "M=M-D", "M=M&D", 
        "M=M|D", "M=!M", "M=-M"
    };
    int found_pos = in_array(command->name, NAMES, COUNT_OF(NAMES));
    if (found_pos != -1)
    {
        strcpy(diff, DIFFS[found_pos]);
    }

    if (command->type == C_PUSH)
    {
        if (strcmp(command->arg1, "constant") == 0)
        {
            sprintf(line, "// %s\n@%s\nD=A\n@SP\nM=M+1\nA=M-1\nM=D\n\n", 
                    command->cln, command->arg2);
        }
        else if (strcmp(command->arg1, "argument") == 0)
        {
            sprintf(line, "// %s\n@%s\nD=A\n@ARG\nA=M+D\nD=M\n"
                          "@SP\nM=M+1\nA=M-1\nM=D\n\n",
                    command->cln, command->arg2);
        }
        else if (strcmp(command->arg1, "local") == 0)
        {
            sprintf(line, "// %s\n@%s\nD=A\n@LCL\nA=M+D\nD=M\n"
                          "@SP\nM=M+1\nA=M-1\nM=D\n\n",
                    command->cln, command->arg2);
        }
        else if (strcmp(command->arg1, "this") == 0)
        {
            sprintf(line, "// %s\n@%s\nD=A\n@THIS\nA=M+D\nD=M\n"
                          "@SP\nM=M+1\nA=M-1\nM=D\n\n",
                    command->cln, command->arg2);
        }
        else if (strcmp(command->arg1, "that") == 0)
        {
            sprintf(line, "// %s\n@%s\nD=A\n@THAT\nA=M+D\nD=M\n"
                          "@SP\nM=M+1\nA=M-1\nM=D\n\n",
                    command->cln, command->arg2);
        }
        else if (strcmp(command->arg1, "pointer") == 0)
        {
            char diff[10];
            if (strcmp(command->arg2, "0") == 0)
                strcpy(diff, "THIS");
            else
                strcpy(diff, "THAT");

            sprintf(line, "// %s\n@%s\nD=M\n@SP\nM=M+1\nA=M-1\nM=D\n\n",
                    command->cln, diff);
        }
        else if (strcmp(command->arg1, "temp") == 0)
        {
            char diff[10], strindex[10];
            int index = atoi(command->arg2);
            itoa(index + 5, strindex, 10);  
            sprintf(diff, "R%s", strindex);  // 0 -> "R5", 7 -> "R12"

            sprintf(line, "// %s\n@%s\nD=M\n@SP\nM=M+1\nA=M-1\nM=D\n\n",
                    command->cln, diff);
        }
    }
    else if (command->type == C_POP)
    {
        if (strcmp(command->arg1, "argument") == 0)
        {
            sprintf(line, "// %s\n@%s\nD=A\n@ARG\nA=M+D\nD=A\n"
                          "@R13\nM=D\n@SP\nM=M-1\nA=M\nD=M\n"
                          "@R13\nA=M\nM=D\n\n",
                    command->cln, command->arg2);
        }
        else if (strcmp(command->arg1, "local") == 0)
        {
            sprintf(line, "// %s\n@%s\nD=A\n@LCL\nA=M+D\nD=A\n"
                          "@R13\nM=D\n@SP\nM=M-1\nA=M\nD=M\n"
                          "@R13\nA=M\nM=D\n\n",
                    command->cln, command->arg2);
        }
        else if (strcmp(command->arg1, "this") == 0)
        {
            sprintf(line, "// %s\n@%s\nD=A\n@THIS\nA=M+D\nD=A\n"
                          "@R13\nM=D\n@SP\nM=M-1\nA=M\nD=M\n"
                          "@R13\nA=M\nM=D\n\n",
                    command->cln, command->arg2);
        }
        else if (strcmp(command->arg1, "that") == 0)
        {
            sprintf(line, "// %s\n@%s\nD=A\n@THAT\nA=M+D\nD=A\n"
                          "@R13\nM=D\n@SP\nM=M-1\nA=M\nD=M\n"
                          "@R13\nA=M\nM=D\n\n",
                    command->cln, command->arg2);
        }
        else if (strcmp(command->arg1, "pointer") == 0)
        {
            char diff[10];
            if (strcmp(command->arg2, "0") == 0)
                strcpy(diff, "THIS");
            else
                strcpy(diff, "THAT");

            sprintf(line, "// %s\n@SP\nM=M-1\nA=M\nD=M\n@%s\nM=D\n\n",
                    command->cln, diff);
        }
        else if (strcmp(command->arg1, "temp") == 0)
        {
            char diff[10], strindex[10];
            int index = atoi(command->arg2);
            itoa(index + 5, strindex, 10);  
            sprintf(diff, "R%s", strindex);  // 0 -> "R5", 7 -> "R12"
            
            sprintf(line, "// %s\n@SP\nM=M-1\nA=M\nD=M\n@%s\nM=D\n\n",
                    command->cln, diff);
        }
    }
    else if (command->type == C_CMP)
    {
        char label[200];
        sprintf(label, "%s.%d.true", filename, line_num);

        sprintf(line, "// %s\n@SP\nAM=M-1\nD=M\nA=A-1\nD=M-D\nM=-1\n"
                      "@%s\n%s\n@SP\nA=M-1\nM=!M\n(%s)\n\n",
                      command->cln, label, diff, label);    
    }
    else if (command->type == C_ARITHMETIC)
    {
        sprintf(line, "// %s\n@SP\nAM=M-1\nD=M\nA=A-1\n%s\n\n", 
                command->cln, diff);
    }
    else if (command->type == C_UNARY)
    {
        sprintf(line, "// %s\n@SP\nA=M-1\nD=M\n%s\n\n", 
                command->cln, diff);
    }
    else if (command->type == C_LABEL ||
             command->type == C_GOTO ||
             command->type == C_IF_GOTO)
    {
        char label[MAXLINE];
        sprintf(label, "%s.%s", filename, command->arg1);

        if (command->type == C_LABEL)
        {
            sprintf(line, "// %s\n(%s)\n\n", command->cln, label);
        }
        else if (command->type == C_GOTO)
        {
            sprintf(line, "// %s\n@%s\n0;JMP\n\n", command->cln, label);
        }
        else if (command->type == C_IF_GOTO)
        {
            sprintf(line, "// %s\n@SP\nAM=M-1\nD=M\n@%s\nD;JNE\n\n", command->cln, label);
        }    
    }
    
    
}


