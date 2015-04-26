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


static Tokens *tokens;
static Symbol_Table *class_scope;
static Symbol_Table *function_scope;
static char global_class_name[MAXLINE];
static FILE *dest_file;


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


Token *new_token()
{
    // Returns a pointer to the next free slot
    // for a parsed command. Allocates space.

    Token *result = 0;

    if (tokens->entries == 0)
    {
        tokens->allocated = 1000;
        tokens->entries = malloc(tokens->allocated * sizeof(Token));
    }
    if (tokens->len >= (tokens->allocated - 2))
    {
        tokens->allocated = tokens->allocated * 2;
        tokens->entries = realloc(tokens->entries,
                                    tokens->allocated * sizeof(Token));
    }
    result = tokens->entries + tokens->len;
    tokens->len++;

    return result;
}


Parse_result parse_line(char *line, char *filename, int line_num,
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

        Token *token = new_token();
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


void print_token(Token *token)
{
    char type_str[MAXLINE], token_str[MAXLINE];

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

    strcpy(token_str, token->repr);
    if (strcmp(token_str, "<") == 0)
    {
        strcpy(token_str, "&lt;");
    }
    else if (strcmp(token_str, ">") == 0)
    {
        strcpy(token_str, "&gt;");
    }
    else if (strcmp(token_str, "&") == 0)
    {
        strcpy(token_str, "&amp;");
    }

    printf("<%s>%s</%s>\n", type_str, token_str, type_str);
}


Token *get_next_token()
{
    return tokens->next++;
}


Token *step_back()
{
    return --tokens->next;
}


void syntax_error(Token *token, char *expected)
{
    printf("%s:%d: error: %s expected, got '%s'\n",
           token->filename, token->line_num,
           expected, token->repr);
    exit(1);
}


Symbol_Table_Entry *new_entry(Symbol_Table *table)
{

    // Returns a pointer to the next free entry.
    // Allocates space

    Symbol_Table_Entry *result = 0;

    if (table->entries == 0)
    {
        table->allocated = 1000;
        table->entries = malloc(table->allocated * sizeof(Symbol_Table_Entry));
    }
    if (table->len >= (table->allocated - 2))
    {
        table->allocated = table->allocated * 2;
        table->entries = realloc(table->entries,
                                 table->allocated * sizeof(Symbol_Table_Entry));
    }
    result = table->entries + table->len;
    table->len++;

    return result;
}


Symbol_Table *new_scope()
{
    Symbol_Table *table = calloc(1, sizeof(Symbol_Table));
    return table;
}


Symbol_Table_Entry *declare_var(Symbol_Table *scope, char *name, char *type, char *kind)
{
    Symbol_Table_Entry *entry = new_entry(scope);

    strcpy(entry->name, name);
    strcpy(entry->type, type);
    if (strcmp(kind, "static") == 0)
        entry->kind = STATIC;
    else if (strcmp(kind, "field") == 0)
        entry->kind = FIELD;
    else if (strcmp(kind, "argument") == 0)
        entry->kind = ARG;
    else if (strcmp(kind, "var") == 0)
        entry->kind = VAR;
    else
    {
        printf("Unknown variable kind: %s\n", kind);
        exit(1);
    }
    int num = 0;
    for (int i = 0; i < scope->len - 1; i++)  // don't look at itself
    {
        if (scope->entries[i].kind == entry->kind)
            num++;
    }
    entry->num = num;

    return entry;
}


Symbol_Table_Entry *find_var(Token *token)
{
    Symbol_Table_Entry *entry = 0;

    // Search in function scope
    if (function_scope)
    {
        for (int i = 0; i < function_scope->len; i++)
        {
            entry = function_scope->entries + i;
            if (strcmp(token->repr, entry->name) == 0)
                return entry;
        }
    }

    // Search in class scope. Train my tolerance to duplicated code
    for (int i = 0; i < class_scope->len; i++)
    {
        entry = class_scope->entries + i;
        if (strcmp(token->repr, entry->name) == 0)
            return entry;
    }

    printf("%s:%d: error: Undefined variable '%s'\n",
           token->filename, token->line_num, token->repr);
    exit(1);
}


void print_var(Symbol_Table_Entry *entry)
{
    char kind[10];
    if (entry->kind == STATIC)
    {
        strcpy(kind, "static");
    }
    else if (entry->kind == FIELD)
    {
        strcpy(kind, "field");
    }
    else if (entry->kind == VAR)
    {
        strcpy(kind, "var");
    }
    else if (entry->kind == ARG)
    {
        strcpy(kind, "argument");
    }
    printf("<var type='%s' kind='%s' num='%d'>%s</var>\n",
           entry->type, kind, entry->num, entry->name);
}


Token *expect(bool mandatory, token_type type, char *repr, char *expected, bool print)
{
    Token *token = get_next_token();
    if (token->type != type || (repr != 0 && strcmp(token->repr, repr) != 0))
    {
        if (mandatory)
        {
            char expected_str[MAXLINE];
            if (type == SYMBOL)
                sprintf(expected_str, "'%s'", expected);
            else
                strcpy(expected_str, expected);
            syntax_error(token, expected_str);
        }
        else
        {
            step_back();
            return 0;
        }
    }
    // Left for debugging
    //
    // if (print)
    // {
    //     print_token(token);
    // }

    return token;
}


// Match helpers

Token *try_keyword(char *repr)
{
    return expect(false, KEYWORD, repr, repr, true);
}

Token *try_symbol(char *repr)
{
    return expect(false, SYMBOL, repr, repr, true);
}

Token *try_identifier()
{
    return expect(false, IDENTIFIER, 0, "identifier", false);
}

Token *try_integer()
{
    return expect(false, INT_CONST, 0, "integer", true);
}

Token *try_string()
{
    return expect(false, STRING_CONST, 0, "string", true);
}

Token *expect_keyword(char *repr)
{
    return expect(true, KEYWORD, repr, repr, true);
}

Token *expect_symbol(char *repr)
{
    return expect(true, SYMBOL, repr, repr, true);
}

Token *expect_identifier()
{
    Token *result = try_identifier();
    if (result == 0)
        syntax_error(get_next_token(), "identifier");
    return result;
}

Token *expect_integer()
{
    return expect(true, INT_CONST, 0, "integer", true);
}

Token *expect_string()
{
    return expect(true, STRING_CONST, 0, "string", true);
}

bool peek_keyword(char *repr)
{
    if (expect(false, KEYWORD, repr, repr, false))
    {
        step_back();
        return true;
    }
    return false;
}

bool peek_symbol(char *repr)
{
    if (expect(false, SYMBOL, repr, repr, false))
    {
        step_back();
        return true;
    }
    return false;
}

bool peek_identifier()
{
    if (expect(false, IDENTIFIER, 0, "identifier", false))
    {
        step_back();
        return true;
    }
    return false;
}


void match_statements();


Token *match_type()
{
    Token *token;

    if ((token = try_keyword("int")) ||
        (token = try_keyword("char")) ||
        (token = try_keyword("boolean")) ||
        (token = try_identifier()))  // class name, used
    {
        return token;
    }

    return 0;
}


Token *expect_type()
{
    Token *token = match_type();
    if (token == 0)
    {
        syntax_error(get_next_token(), "type");
    }
    return token;
}


void match_class_var_dec()
{
    Token *token, *type, *kind;
    Symbol_Table_Entry *var;

    kind = try_keyword("field");
    if (!kind)
        kind = expect_keyword("static");
    type = expect_type();

    token = expect_identifier();
    var = declare_var(class_scope, token->repr, type->repr, kind->repr);

    while (try_symbol(","))
    {
        token = expect_identifier();
        var = declare_var(class_scope, token->repr, type->repr, kind->repr);
    }
    expect_symbol(";");
}


int match_parameter_list()
{
    Token *token, *type;
    int param_num = 0;

    type = match_type();
    if (type)
    {
        token = expect_identifier();
        declare_var(function_scope, token->repr, type->repr, "argument");
        param_num++;

        while (try_symbol(","))
        {
            type = expect_type();
            token = expect_identifier();
            declare_var(function_scope, token->repr, type->repr, "argument");
            param_num++;
        }
    }

    return param_num;
}


bool match_op()
{
    printf("<op>\n");
    bool match = (
        try_symbol("+") ||
        try_symbol("-") ||
        try_symbol("*") ||
        try_symbol("/") ||
        try_symbol("&") ||
        try_symbol("|") ||
        try_symbol("<") ||
        try_symbol(">") ||
        try_symbol("=")
    );
    printf("</op>\n");

    return match;
}


bool match_unary_op()
{
    printf("<unaryOp>\n");
    bool match = try_symbol("-") || try_symbol("~");
    printf("</unaryOp>\n");
    return match;
}


bool match_keyword_constant()
{
    return (try_keyword("true") || try_keyword("false") ||\
            try_keyword("null") || try_keyword("this"));
}


void expect_term();
bool match_expression();
void match_subroutine_call();


bool match_term()
{
    printf("<term>\n");
    bool match = false;

    if (try_integer() || try_string() || match_keyword_constant())
    {
        match = true;
    }
    else if (try_symbol("("))
    {
        match_expression();
        expect_symbol(")");
        match = true;
    }
    else if (peek_identifier())
    {
        match = true;

        Token *token;
        Symbol_Table_Entry *var;

        get_next_token();
        if (peek_symbol("["))
        {
            // varName '[' expression ']'
            step_back();
            token = expect_identifier();
            var = find_var(token);
            print_var(var);
            expect_symbol("[");
            match_expression();
            expect_symbol("]");
        }
        else if (peek_symbol("(") || peek_symbol("."))
        {
            step_back();  // before the identifier
            match_subroutine_call();
        }
        else
        {
            // varName
            step_back();
            token = expect_identifier();
            var = find_var(token);
            print_var(var);
        }
    }
    else if (match_unary_op())
    {
        expect_term();
        match = true;
    }

    printf("</term>\n");
    return match;
}

void expect_term()
{
    if (!match_term())
    {
        syntax_error(get_next_token(), "type");
    }
}


bool match_expression()
{
    printf("<expression>\n");
    bool match = false;
    if (match_term())
    {
        while (match_op())
        {
            expect_term();
        }
        match = true;
    }

    printf("</expression>\n");
    return match;
}


void expect_expression()
{
    if (!match_expression())
    {
        syntax_error(get_next_token(), "expression");
    }
}


void match_expression_list()
{
    if (match_expression())
    {
        while (try_symbol(","))
        {
            expect_expression();
        }
    }
}


void match_subroutine_call()
{
    printf("<subroutineCall>\n");

    Token *token;

    get_next_token();  // Should be an identifier
    if (peek_symbol("."))
    {
        step_back();
        token = expect_identifier();  // class name
        printf("<class_name>%s</class_name>\n", token->repr);

        expect_symbol(".");
        token = expect_identifier();  // function name
        printf("<function_name>%s</function_name>\n", token->repr);
    }
    else
    {
        step_back();
        token = expect_identifier();  // function name
        printf("<function_name>%s</function_name>\n", token->repr);
    }

    expect_symbol("(");
    match_expression_list();
    expect_symbol(")");

    printf("</subroutineCall>\n");
}


void match_let()
{
    printf("<let>\n");

    Token *token;
    Symbol_Table_Entry *var;

    expect_keyword("let");
    token = expect_identifier();
    var = find_var(token);
    print_var(var);

    if (try_symbol("["))
    {
        expect_expression();
        expect_symbol("]");
    }
    expect_symbol("=");
    expect_expression();
    expect_symbol(";");

    printf("</let>\n");
}


void match_if()
{
    printf("<if>\n");

    expect_keyword("if");
    expect_symbol("(");
    expect_expression();
    expect_symbol(")");
    expect_symbol("{");
    match_statements();
    expect_symbol("}");
    if (try_keyword("else"))
    {
        expect_symbol("{");
        match_statements();
        expect_symbol("}");
    }

    printf("</if>\n");
}


void match_while()
{
    printf("<while>\n");

    expect_keyword("while");
    expect_symbol("(");
    expect_expression();
    expect_symbol(")");
    expect_symbol("{");
    match_statements();
    expect_symbol("}");
    printf("</while>\n");
}


void match_do()
{
    printf("<do>\n");

    expect_keyword("do");
    match_subroutine_call();
    expect_symbol(";");
    printf("</do>\n");
}


void match_return()
{
    printf("<return>\n");

    expect_keyword("return");
    match_expression();
    expect_symbol(";");

    printf("</return>\n");
}


void match_statements()
{
    printf("<statements>\n");

    bool none_matched = false;
    while (!none_matched)
    {
        if (peek_keyword("let"))
            match_let();
        else if (peek_keyword("if"))
            match_if();
        else if (peek_keyword("while"))
            match_while();
        else if (peek_keyword("do"))
            match_do();
        else if (peek_keyword("return"))
            match_return();
        else
            none_matched = true;
    }

    printf("</statements>\n");
}


void match_var_dec()
{
    printf("<varDec>\n");

    Token *token, *type;
    Symbol_Table_Entry *var;

    expect_keyword("var");
    type = expect_type();
    token = expect_identifier();
    var = declare_var(function_scope, token->repr, type->repr, "var");
    print_var(var);

    while (try_symbol(","))
    {
        token = expect_identifier();
        var = declare_var(function_scope, token->repr, type->repr, "var");
        print_var(var);
    }
    expect_symbol(";");

    printf("</varDec>\n");
}


void match_subroutine_body()
{
    printf("<subroutineBody>\n");

    expect_symbol("{");
    while (peek_keyword("var"))
        match_var_dec();
    match_statements();
    expect_symbol("}");

    printf("</subroutineBody>\n");
}


void match_subroutine_dec()
{
    if (function_scope)
    {
        // Free the scope of the previous function
        free(function_scope->entries);
        free(function_scope);
    }
    function_scope = new_scope();

    if (try_keyword("constructor"))
    {
        // Constructor
    }
    else if (try_keyword("function"))
    {
        // Function
    }
    else if (expect_keyword("method"))
    {
        // Method
    }

    try_keyword("void") || expect_type();  // TODO: maybe check return type

    Token *token = expect_identifier();  // function name, declared
    expect_symbol("(");
    int param_num = match_parameter_list();
    expect_symbol(")");

    // Write function header
    fprintf(dest_file, "function %s.%s %d\n", global_class_name, token->repr, param_num);

    match_subroutine_body();
}


void expect_class()
{
    class_scope = new_scope();  // never freed

    Token *token;

    expect_keyword("class");
    token = expect_identifier();  // class name, declared
    strcpy(global_class_name, token->repr);

    expect_symbol("{");

    while (peek_keyword("static") ||
           peek_keyword("field"))
    {
        match_class_var_dec();
    }
    while (peek_keyword("constructor") ||
           peek_keyword("function") ||
           peek_keyword("method"))
    {
        match_subroutine_dec();
    }

    expect_symbol("}");
}


void compile_file(char *file_path)
{
    // Allocate memory for tokens
    tokens = calloc(1, sizeof(Tokens));

    int line_num = 0;
    char line[MAXLINE];
    char filename[MAXLINE];
    char dest_file_path[MAXLINE];

    get_name_from_path(file_path, filename);
    get_dir_path(file_path, dest_file_path);
    snprintf(dest_file_path, sizeof(dest_file_path), "%s/%s.vm", dest_file_path, filename);

    // Open source file
    FILE *file = fopen(file_path, "rb");
    if (file == NULL)
        return;

    // Open dest file
    dest_file = fopen(dest_file_path, "wb");
    if (dest_file == NULL)
    {
        printf("Cannot open dest file %s\n", dest_file_path);
        exit(1);
    }

    Parse_result result;
    bool multi_line_comment_mode = false;

    while (fgets(line, MAXLINE, file))
    {
        line_num++;

        result = parse_line(line, file_path, line_num, &multi_line_comment_mode);
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

    // Parse syntax
    tokens->next = tokens->entries;  // This is the root node, start from the first
    expect_class();

    free(tokens);
    fclose(file);
    fclose(dest_file);

    printf("File %s written\n", dest_file_path);
}


