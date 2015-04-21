#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <ctype.h>

#include "compiler.h"


void compile_file(char *filename);
bool file_extension_is_jack(char *file_path);
int is_directory(char *path);
void get_dir_path(char *source_path, char *dir_path);


int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("No source file/dir specified\n");
        return 1;
    }

    char *source_path = argv[1];

    if (!is_directory(source_path))
    {
        if (!file_extension_is_jack(source_path))
        {
            printf("Wrong file extension\n");
            exit(1);
        }
        compile_file(source_path);
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
            if (!file_extension_is_jack(dir->d_name))
                continue;

            char dest_dir_path[MAXLINE];
            char file_path[MAXLINE];
            get_dir_path(source_path, dest_dir_path);
            sprintf(file_path, "%s/%s", dest_dir_path, dir->d_name);
            compile_file(file_path);
        }
        closedir(d);
    }

    return 0;
}


bool file_extension_is_jack(char *file_path)
{
    char *extension = strrchr(file_path, '.');
    if (extension == NULL || strcmp(extension, ".jack") != 0)
        return false;
    else
        return true;
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


void clean_line(char *line, bool *multi_line_comment_mode)
{
    // Deletes multiple spaces, comments, and trims
    // whitespace on both sides

    char *cln = line, *beginning = line;
    int space_num = 0;
    int non_space_num = 0;

    while (*line != 0)
    {
        if (*multi_line_comment_mode)
        {
            if (*line == '*' && *(line + 1) == '/')
            {
                *multi_line_comment_mode = false;
                line = line + 2;
            }
            else
            {
                line++;
            }
            continue;
        }

        if (*line == '/' && *(line + 1) == '*')
        {
            *multi_line_comment_mode = true;
            line = line + 2;
            continue;
        }

        if (isblank(*line) && (!non_space_num || space_num > 1))
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
        if (isblank(*line))
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


int string_in_array(char *string, char *array[], int len)
{
    // Returns the index of the first occurrence
    // of 'string' in the array or -1

    for (int i = 0; i < len; i++)
    {
        if (strcmp(array[i], string) == 0)
            return i;
    }
    return -1;
}


int char_in_array(char c, char array[], int len)
{
    // Returns the index of the first occurrence
    // of the char in the array or -1

    for (int i = 0; i < len; i++)
    {
        if (c == array[i])
            return i;
    }
    return -1;
}


Token *new_token(Tokens *list)
{
    // Returns a pointer to the next free slot
    // for a parsed command. Allocates space.

    Token *result = 0;

    if (list->entries == 0)
    {
        list->allocated = 1000;
        list->entries = malloc(list->allocated * sizeof(Token));
    }
    if (list->len >= (list->allocated - 2))
    {
        list->allocated = list->allocated * 2;
        list->entries = realloc(list->entries,
                                    list->allocated * sizeof(Token));
    }
    result = list->entries + list->len;
    list->len++;

    return result;
}


Parse_result parse_line(char *line, Tokens *tokens, char *filename, int line_num,
                        bool *multi_line_comment_mode)
{
    Parse_result result = {};

    clean_line(line, multi_line_comment_mode);

    if (strlen(line) == 0)
        return result;  // PARSE_BLANK

    char *KEYWORDS[] = {
        "class", "constructor", "function", "method",
        "field", "static", "var", "int", "char", "boolean",
        "void", "true", "false", "null", "this", "let",
        "do", "if", "else", "while", "return"
    };

    char SYMBOLS[] = {
        '{', '}', '(', ')', '[', ']', '.', ',', ';', '+',
        '-', '*', '/', '&', '|', '<', '>', '=', '~'
    };

    char *cursor = line;
    while (*cursor != '\0')
    {
        char c = *cursor;

        if (isblank(c))
        {
            cursor++;
            continue;  // Ignore whitespace
        }

        Token *token = new_token(tokens);
        token->filename = filename;
        token->line_num = line_num;
        char *write_cursor = &token->repr[0];

        if (char_in_array(c, SYMBOLS, COUNT_OF(SYMBOLS)) >= 0)
        {
            token->type = SYMBOL;
            *write_cursor++ = c;
            *write_cursor = '\0';  // '{' -> "{"
            cursor++;
            continue;
        }
        else if (isdigit(c))
        {
            token->type = INT_CONST;
            while ((c = *cursor) != '\0' && isdigit(c))
            {
                *write_cursor++ = c;
                cursor++;
            }
            *write_cursor = '\0';
            continue;
        }
        else if (c == '"')
        {
            token->type = STRING_CONST;
            while ((c = *(cursor + 1)) != '"')
            {
                if (c == '\0')
                {
                    result.code = PARSE_ERROR;
                    strcpy(result.message, "Unmatched double quote");
                    return result;
                }
                *write_cursor++ = c;
                cursor++;
            }
            *write_cursor = '\0';
            cursor += 2;  // Skip the next double quote
            continue;
        }
        else if (isalnum(c) || c == '_')
        {
            // It is either a keyword or an identifier
            while ((c = *cursor) != '\0' && ((isalnum(c) || c == '_')))
            {
                *write_cursor++ = c;
                cursor++;
            }
            *write_cursor = '\0';

            if (string_in_array(token->repr, KEYWORDS, COUNT_OF(KEYWORDS)) >= 0)
                token->type = KEYWORD;
            else
                token->type = IDENTIFIER;
        }
        else
        {
            result.code = PARSE_ERROR;
            sprintf(result.message, "Illegal token: %c\n", c);
            return result;
        }
    }

    return result;
}


Token *get_next_token(Tokens *tokens)
{
    Token *result = tokens->next;
    tokens->next++;
    return result;
}


Token *expect(Tokens *tokens, bool mandatory, token_type type, char *repr, char *expected_type)
{
    Token *token = get_next_token(tokens);
    if (token->type != type || (repr != 0 && strcmp(token->repr, repr) != 0))
    {
        if (mandatory)
        {
            printf("%s:%d: error: %s expected, got '%s'\n",
                   token->filename, token->line_num,
                   expected_type, token->repr);
            exit(1);
        }
        return 0;
    }
    return token;
}


Token *expect_keyword(Tokens *tokens, char *repr, bool mandatory)
{
    return expect(tokens, mandatory, KEYWORD, repr, repr);
}

Token *expect_symbol(Tokens *tokens, char *repr, bool mandatory)
{
    return expect(tokens, mandatory, SYMBOL, repr, repr);
}

Token *expect_identifier(Tokens *tokens, bool mandatory)
{
    return expect(tokens, mandatory, IDENTIFIER, 0, "identifier");
}

Token *expect_integer(Tokens *tokens, bool mandatory)
{
    return expect(tokens, mandatory, INT_CONST, 0, "integer");
}

Token *expect_string(Tokens *tokens, bool mandatory)
{
    return expect(tokens, mandatory, STRING_CONST, 0, "string");
}


void print_token(Token *token)
{
    char type_str[MAXLINE];

    if (token->type == KEYWORD)
        strcpy(type_str, "keyword");
    else if (token->type == SYMBOL)
        strcpy(type_str, "symbol");
    else if (token->type == IDENTIFIER)
        strcpy(type_str, "identifier");
    else if (token->type == INT_CONST)
        strcpy(type_str, "integerConstant");
    else if (token->type == STRING_CONST)
        strcpy(type_str, "stringConstant");

    printf("<%s>%s</%s>\n", type_str, token->repr, type_str);
}


// Match prototypes
void match_class_var_dec(Tokens *tokens);
void match_class_subroutine_dec(Tokens *tokens);


void match_class(Tokens *tokens)
{
    printf("<class>\n");

    Token *token = get_next_token(tokens);
    if (token->type != KEYWORD || strcmp(token->repr, "class") != 0)
    {
        printf("class expected\n");
        exit(1);
    }
    print_token(token);

    token = get_next_token(tokens);
    if (token->type != IDENTIFIER)
    {
        printf("class name expected\n");
        exit(1);
    }
    print_token(token);

    token = get_next_token(tokens);
    if (token->type != SYMBOL || strcmp(token->repr, "{") != 0)
    {
        printf("{ expected\n");
        exit(1);
    }
    print_token(token);

    if (strcmp(tokens->next->repr, "static") == 0 ||
        strcmp(tokens->next->repr, "field") == 0)
    {
        get_next_token(tokens);
        match_class_var_dec(tokens);
    }
    if (strcmp(tokens->next->repr, "constructor") == 0 ||
        strcmp(tokens->next->repr, "function") == 0 ||
        strcmp(tokens->next->repr, "method") == 0)
    {
        get_next_token(tokens);
        match_class_subroutine_dec(tokens);
    }

    token = get_next_token(tokens);
    if (token->type != SYMBOL || strcmp(token->repr, "}") != 0)
    {
        printf("} expected\n");
        exit(1);
    }
    print_token(token);

    printf("</class>\n");
}


void match_class_var_dec(Tokens *tokens)
{
    printf("<classVarDec>\n");
    Token *token = get_next_token(tokens);
    printf("</classVarDec>\n");

}


void match_class_subroutine_dec(Tokens *tokens)
{
    printf("<subroutineDec>\n");
    Token *token = get_next_token(tokens);
    printf("</subroutineDec>\n");
}


void compile_file(char *file_path)
{
    Tokens tokens = {};
    // Tokenize
    {
        int line_num = 0;
        char line[MAXLINE];
        char filename[MAXLINE];

        get_name_from_path(file_path, filename);

        FILE *file = fopen(file_path, "rb");
        if (file == NULL)
            return;

        Parse_result result;
        bool multi_line_comment_mode = false;

        while (fgets(line, MAXLINE, file))
        {
            line_num++;

            result = parse_line(line, &tokens, file_path, line_num, &multi_line_comment_mode);
            if (result.code == PARSE_BLANK)
                continue;
            if (result.code == PARSE_ERROR)
            {
                printf("Syntax error at line %d in file %s: \n%s\n",
                        line_num, file_path, line);
                printf("%s\n", result.message);
                exit(1);
            }
        }

        fclose(file);
    }

    // Write file
    {
        // char dest_file_path[MAXLINE];
        // // path/to/file.jack -> path/to/file.vm
        // strcpy(dest_file_path, file_path);
        // char *extension = strrchr(dest_file_path, '.');
        // strcpy(extension, ".vm");
        // FILE *dest_file = fopen(dest_file_path, "wb");

        // // Write XML
        // Token *token;
        // char type_str[30];
        // fprintf(dest_file, "<tokens>\n");
        // for (int i = 0; i < tokens.len; i++)
        // {
        //     token = &tokens.entries[i];
        //     if (token->type == KEYWORD)
        //         strcpy(type_str, "keyword");
        //     else if (token->type == SYMBOL)
        //         strcpy(type_str, "symbol");
        //     else if (token->type == IDENTIFIER)
        //         strcpy(type_str, "identifier");
        //     else if (token->type == INT_CONST)
        //         strcpy(type_str, "integerConstant");
        //     else if (token->type == STRING_CONST)
        //         strcpy(type_str, "stringConstant");

        //     fprintf(dest_file, "<%s> %s </%s>\n", type_str, token->repr, type_str);
        // }
        // fprintf(dest_file, "</tokens>\n");

        // fclose(dest_file);
        // printf("File %s written.\n", dest_file_path);
    }

    // Parse syntax
    tokens.next = tokens.entries;  // This is the root node, start from the first
    match_class(&tokens);
}


