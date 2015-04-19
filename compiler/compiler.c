#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdbool.h>

#include "compiler.h"


void compile_file(char *filename);
parse_result parse_line(char *line, Tokens *tokens);
bool file_extension_is_jack(char *file_path);
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


void compile_file(char *file_path)
{
    printf("Compiling %s\n", file_path);

    Tokens *tokens;
    // Tokenize
    {
        int line_num = 0;
        char line[MAXLINE];
        char filename[MAXLINE];

        get_name_from_path(file_path, filename);

        FILE *file = fopen(file_path, "rb");
        if (file == NULL)
            return;

        while (fgets(line, MAXLINE, file))
        {
            line_num++;

            result = parse_line(line, tokens);
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

    // Write file
    {
        char dest_file_path[MAXLINE];
        // path/to/file.jack -> path/to/file.vm
        strcpy(dest_file_path, file_path);
        char *extension = strrchr(dest_file_path, '.');
        strcpy(extension, ".vm");
        FILE *dest_file = fopen(dest_file_path, "wb");

        // TODO: write XML
        fputs("Aha!", dest_file);

        printf("File %s written.\n", dest_file_path);
        fclose(dest_file);
    }
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


parse_result parse_line(char *line, Tokens *tokens)
{
    parse_result result = {};

    clean_line(line);

    if (strlen(line) == 0)
        return result;  // PARSE_BLANK



    return result;
}
