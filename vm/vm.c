#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#include "vm.h"


parse_result parse_line(char *line, ParsedCommands *commands, char *enc_fn);
void parse_file(char *filename, ParsedCommands *commands);
void encode(char *line, Command *command, int line_num);
int is_directory(char *path);
void get_dir_path(char *source_path, char *dir_path);
void get_name_from_path(char *source_path, char *name);



int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("No source file/dir specified\n");
        return 1;
    }

    ParsedCommands commands = {};

    char *source_path = argv[1];
    char dest_dir_path[MAXLINE];
    get_dir_path(source_path, dest_dir_path);

    if (!is_directory(source_path))
    {
        // Check extension
        {
            char *extension = strrchr(source_path, '.');
            if (strcmp(extension, ".vm") != 0)
            {
                printf("Wrong file extension %s\n", extension);
                exit(1);
            }
        }
        parse_file(source_path, &commands);
    }
    else
    {
        DIR *d = opendir(source_path);
        if (d == NULL)
        {
            printf("Cannot open %s\n", source_path);
            return 1;
        }

        struct dirent *dir;
        while ((dir = readdir(d)) != NULL)
        {
            char *extension = strrchr(dir->d_name, '.');
            if (extension == NULL) continue;
            if (strcmp(extension, ".vm") != 0) continue;

            char file_path[MAXLINE];
            sprintf(file_path, "%s/%s", dest_dir_path, dir->d_name);
            parse_file(file_path, &commands);
        }
        closedir(d);
    }

    // Write file
    char dest_file_path[MAXLINE];
    char dest_file_name[MAXLINE];
    get_name_from_path(source_path, dest_file_name);
    sprintf(dest_file_path, "%s/%s.asm", dest_dir_path, dest_file_name);

    FILE *dest = fopen(dest_file_path, "wb");
    char code_line[MAXLINE];

    if (is_directory(source_path))
    {
        // Bootstrap code
        fputs("// Bootstrap\n\n// SP = 256\n@256\nD=A\n@SP\nM=D\n\n"
              "// call Sys.init\n"
              "// push ret addr\n@0\nD=A\n@SP\nM=M+1\nA=M-1\nM=D\n"
              "// push LCL\n@LCL\nD=M\n@SP\nM=M+1\nA=M-1\nM=D\n"
              "// push ARG\n@ARG\nD=M\n@SP\nM=M+1\nA=M-1\nM=D\n"
              "// push THIS\n@THIS\nD=M\n@SP\nM=M+1\nA=M-1\nM=D\n"
              "// push THAT\n@THAT\nD=M\n@SP\nM=M+1\nA=M-1\nM=D\n"
              "// ARG = SP-n-5\n@SP\nD=M\n@0\nD=D-A\n@5\nD=D-A\n@ARG\nM=D\n"
              "// LCL = SP\n@SP\nD=M\n@LCL\nM=D\n"
              "// goto sys.init\n@Sys.init$label\n0;JMP\n"
              "\n",
              dest);
    }

    // Encode lines
    for (int i = 0; i < commands.len; i++)
    {
        encode(code_line, &commands.entries[i], i);
        fputs(code_line, dest);
    }

    fclose(dest);
    printf("File %s written\n", dest_file_path);

    return 0;
}


char *get_last_slash(char *source_path)
{
    char *last_slash = strrchr(source_path, '/');  // Fuck Windows
    if (source_path[strlen(source_path) - 1] == '/')
    {
        *last_slash = '\0';
        last_slash = strrchr(source_path, '/');
    }
    return last_slash;
}


void get_dir_path(char *source_path, char *dir_path)
{
    // path/to/dir[/] -> "path/to/dir"
    // path/to/file.ext -> "path/to"

    strcpy(dir_path, source_path);
    char *last_slash = get_last_slash(dir_path);

    if (!is_directory(dir_path))
        *last_slash = '\0';
}


void get_name_from_path(char *source_path, char *name)
{
    // path/to/file.ext -> "file"
    // path/to/dir[/] -> "dir"

    char *last_slash = get_last_slash(source_path);
    strcpy(name, last_slash + 1);

    char *extension = strrchr(name, '.');
    if (extension != NULL)
        *extension = '\0';
}


int is_directory(char *path)
{
    struct stat info;

    if (stat(path, &info) == -1)
    {
        printf("Cannot access path %s\n", path);
        exit(1);
    }

    return S_ISDIR(info.st_mode);
}


void parse_file(char *file_path, ParsedCommands *commands)
{
    printf("Parsing %s\n", file_path);

    FILE *file = fopen(file_path, "rb");
    if (file == NULL)
        return;

    int line_num = 0;
    char line[MAXLINE];
    parse_result result;

    // Get filename for unique labels
    char *filename = malloc(sizeof(char) * MAXLINE);  // never freed
    get_name_from_path(file_path, filename);

    while (fgets(line, MAXLINE, file))
    {
        line_num++;

        result = parse_line(line, commands, filename);
        if (result.code == PARSE_BLANK)
            continue;
        if (result.code == PARSE_ERROR)
        {
            printf("Syntax error at line %d in file %s: \n%s\n",
                    line_num, filename, line);
            printf("%s\n", result.message);
            exit(1);
        }
    }

    fclose(file);
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
    // Returns a pointer to the next free slot
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
             command->type == C_UNARY ||
             command->type == C_RETURN)
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
    else if (command->type == C_CALL)
    {
        if (strcmp(command->arg1, "") == 0 || strcmp(command->arg2, "") == 0)
        {
            strcpy(result->message, "Argument required");
            return PARSE_ERROR;
        }
    }
    else if (command->type == C_FUNCTION)
    {
        if (strcmp(command->arg1, "") == 0 ||
            strcmp(command->arg2, "") == 0)
        {
            strcpy(result->message, "Function definition is incorrect");
            return PARSE_ERROR;
        }
        int arg_num = atoi(command->arg2);
        if ((arg_num == 0 && command->arg2[0] != '0') || arg_num < 0)
        {
            strcpy(result->message, "Invalid number of function arguments");
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


parse_result parse_line(char *line, ParsedCommands *commands, char *enc_fn)
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
        "push", "pop", "label", "goto", "if-goto", "function",
        "call", "return"
    };
    int TYPES[] = {
        C_PUSH, C_POP, C_LABEL, C_GOTO, C_IF_GOTO,
        C_FUNCTION, C_CALL, C_RETURN
    };

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

    command->enc_fn = enc_fn;

    result.code = validate_arguments(command, &result);
    return result;
}


void encode(char *line,  Command *command, int line_num)
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
        else if (strcmp(command->arg1, "static") == 0)
        {
            sprintf(line, "// %s\n@%s.static.%s\nD=M\n@SP\nM=M+1\nA=M-1\nM=D\n\n",
                    command->cln, command->enc_fn, command->arg2);
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
        else if (strcmp(command->arg1, "static") == 0)
        {
            sprintf(line, "// %s\n@SP\nAM=M-1\nD=M\n@%s.static.%s\nM=D\n\n",
                    command->cln, command->enc_fn, command->arg2);
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
        sprintf(label, "%s.%d.true", command->enc_fn, line_num);

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
        sprintf(label, "%s.%s", command->enc_fn, command->arg1);

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
    else if (command->type == C_FUNCTION)
    {
        int arg_num = atoi(command->arg2);

        sprintf(line, "// %s\n(%s$label)\n", command->cln, command->arg1);
        for (int i = 0; i < arg_num; i++)
        {
            strcat(line, "@SP\nM=M+1\nA=M-1\nM=0\n");  // push 0
        }
        strcat(line, "\n");
    }
    else if (command->type == C_CALL)
    {
        char return_address[MAXLINE];
        sprintf(return_address, "%s$return$%d", command->enc_fn, line_num);

        sprintf(
            line,
            "// %s\n"
            "// push ret addr\n@%s\nD=A\n@SP\nM=M+1\nA=M-1\nM=D\n"
            "// push LCL\n@LCL\nD=M\n@SP\nM=M+1\nA=M-1\nM=D\n"
            "// push ARG\n@ARG\nD=M\n@SP\nM=M+1\nA=M-1\nM=D\n"
            "// push THIS\n@THIS\nD=M\n@SP\nM=M+1\nA=M-1\nM=D\n"
            "// push THAT\n@THAT\nD=M\n@SP\nM=M+1\nA=M-1\nM=D\n"
            "// ARG = SP-n-5\n@SP\nD=M\n@%s\nD=D-A\n@5\nD=D-A\n@ARG\nM=D\n"
            "// LCL = SP\n@SP\nD=M\n@LCL\nM=D\n"
            "// goto f\n@%s$label\n0;JMP\n"
            "// return address\n(%s)\n"
            "\n",
            command->cln, return_address, command->arg2,
            command->arg1, return_address
        );
    }
    else if (command->type == C_RETURN)
    {
        sprintf(
            line,
            "// %s\n"
            "// RET = *(FRAME - 5)\n@LCL\nD=M\n@5\nA=D-A\nD=M\n@R13\nM=D\n"
            "// *ARG = pop()\n@SP\nA=M-1\nD=M\n@ARG\nA=M\nM=D\n"
            "// SP = ARG + 1\n@ARG\nD=M+1\n@SP\nM=D\n"
            "// THAT = *(FRAME - 1)\n@LCL\nA=M-1\nD=M\n@THAT\nM=D\n"
            "// THIS = *(FRAME - 2)\n@LCL\nD=M\n@2\nA=D-A\nD=M\n@THIS\nM=D\n"
            "// ARG = *(FRAME - 3)\n@LCL\nD=M\n@3\nA=D-A\nD=M\n@ARG\nM=D\n"
            "// LCL = *(FRAME - 4)\n@LCL\nD=M\n@4\nA=D-A\nD=M\n@LCL\nM=D\n"
            "// goto RET\n@R13\nA=M\n0;JMP\n"
            "\n",
            command->cln
        );
    }
}


