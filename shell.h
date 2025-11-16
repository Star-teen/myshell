#ifndef SHELL_H
#define SHELL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

#define MAX_INPUT_LENGTH 4096
#define MAX_WORDS 100
#define MAX_PATH_LENGTH 1024

// Структура для хранения разобранной команды
typedef struct {
    char **words;
    int word_num;
    int fonius;      // Фоновый режим
    char *input_file;    // Файл для ввода
    char *output_file;   // Файл для вывода stdout
    char *error_file;    // НОВОЕ: Файл для вывода stderr
    int append_output;   // Добавление в файл для stdout
    int append_error;    // НОВОЕ: Добавление в файл для stderr
    int merge_output;    // НОВОЕ: Объединение stdout и stderr (2>&1)
    char ***pipeline;    // Команды в конвейере
    int pipeline_count;  // Количество команд в конвейере
} command_t;


typedef struct {
    command_t **commands;    // Массив команд
    int command_count;       // Количество команд в последовательности
    int *separators;         // Разделители между командами (0 - ;, 1 - &&, 2 - ||)
} command_sequence_t;

// Функции парсера
command_t *parse_input(const char *input);
command_sequence_t *parse_input_with_separators(const char *input);
void free_command(command_t *cmd);
void free_command_sequence(command_sequence_t *seq);

// Функции исполнителя
int execute_command(command_t *cmd);
int execute_command_sequence(command_sequence_t *seq);
int execute_bash_cmd(char **args);
int execute_fonius(command_t *cmd);
int execute_pipeline(command_t *cmd);

// Встроенные команды
int from_bash_cd(char **args);
int from_bash_exit(char **args);
int from_path(char **args);
int set_path(char **args); 
int add_to_path(char **args);
int reset_path(char **args);

void print_command(const command_t *cmd);
void print_command_sequence(const command_sequence_t *seq);

// Утилиты
char *read_line(void);
char **split_line(char *line);
char *get_full_path(const char *command);

// Функции для работы с разделителями команд
int is_command_separator(const char *str);
int get_separator_type(const char *sep);

#endif