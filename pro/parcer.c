#include "shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static const char DELIMITERS[] = " \t\n\r";

void process_redirections_and_pipes(command_t *cmd, char **words, int *word_num);

int is_special_char(char c) {
    const char *special_chars = "@#%!&$^;:,(){}[]";
    return (strchr(special_chars, c) != NULL);
}

int is_redirection_char(char *str) {
    return (strcmp(str, ">") == 0 || strcmp(str, ">>") == 0 || 
            strcmp(str, "<") == 0 || strcmp(str, "2>") == 0 || 
            strcmp(str, "2>>") == 0 || strcmp(str, "2>&1") == 0);
}

int is_command_separator(const char *str) {
    return (strcmp(str, ";") == 0 || strcmp(str, "&&") == 0 || strcmp(str, "||") == 0);
}

int get_separator_type(const char *sep) {
    if (strcmp(sep, ";") == 0) return 0;
    if (strcmp(sep, "&&") == 0) return 1;
    if (strcmp(sep, "||") == 0) return 2;
    return -1;
}

command_t *parse_input(const char *input) {
    if (input == NULL || strlen(input) == 0) {
        return NULL;
    }

    command_t *cmd = malloc(sizeof(command_t));
    if (cmd == NULL) {
        perror("malloc");
        return NULL;
    }

    // Инициализация структуры с новыми полями
    cmd->words = NULL;
    cmd->word_num = 0;
    cmd->fonius = 0;
    cmd->input_file = NULL;
    cmd->output_file = NULL;
    cmd->error_file = NULL;    
    cmd->append_output = 0;
    cmd->append_error = 0;     
    cmd->merge_output = 0;     
    cmd->pipeline = NULL;
    cmd->pipeline_count = 0;

    char *line = strdup(input);
    if (line == NULL) {
        perror("strdup");
        free(cmd);
        return NULL;
    }

    char *ptr = line;
    int in_quotes = 0;
    int escape_next = 0;
    char current_token[MAX_INPUT_LENGTH];
    int token_pos = 0;

    // Временный список токенов
    char **temp_words = malloc(MAX_WORDS * sizeof(char*));
    if (temp_words == NULL) {
        perror("malloc");
        free(line);
        free(cmd);
        return NULL;
    }
    int temp_count = 0;

    while (*ptr != '\0' && temp_count < MAX_WORDS) {
        // Обработка экранирования
        if (escape_next) {
            if (token_pos < MAX_INPUT_LENGTH - 1) {
                current_token[token_pos++] = *ptr;
            }
            escape_next = 0;
            ptr++;
            continue;
        }

        // Обработка обратного слеша для экранирования
        if (*ptr == '\\' && !in_quotes) {
            escape_next = 1;
            ptr++;
            continue;
        }

        // Обработка кавычек
        if (*ptr == '"') {
            in_quotes = !in_quotes;
            ptr++;
            continue;
        }

        if (in_quotes) {
            // Внутри кавычек - все символы обычные
            if (token_pos < MAX_INPUT_LENGTH - 1) {
                current_token[token_pos++] = *ptr;
            }
            ptr++;
        } else {
            // ПРОВЕРКА НА ДВОЙНЫЕ СИМВОЛЫ ПЕРЕНАПРАВЛЕНИЯ В ПЕРВУЮ ОЧЕРЕДЬ
            // Проверка на >> (должна быть ПЕРЕД проверкой на одиночный >)
            if (*ptr == '>' && *(ptr + 1) == '>') {
                // Сохраняем текущий токен, если есть
                if (token_pos > 0) {
                    current_token[token_pos] = '\0';
                    if (temp_count < MAX_WORDS - 1) {
                        temp_words[temp_count++] = strdup(current_token);
                    }
                    token_pos = 0;
                }
                
                // Сохраняем >> как один токен
                if (temp_count < MAX_WORDS - 1) {
                    temp_words[temp_count++] = strdup(">>");
                }
                
                ptr += 2; // Пропускаем два символа
                continue;
            }
            
            // Проверка на << (heredoc)
            if (*ptr == '<' && *(ptr + 1) == '<') {
                // Сохраняем текущий токен, если есть
                if (token_pos > 0) {
                    current_token[token_pos] = '\0';
                    if (temp_count < MAX_WORDS - 1) {
                        temp_words[temp_count++] = strdup(current_token);
                    }
                    token_pos = 0;
                }
                
                if (temp_count < MAX_WORDS - 1) {
                    temp_words[temp_count++] = strdup("<<");
                }
                
                ptr += 2;
                continue;
            }
            
            // Проверка на двойные символы (&& и ||)
            if ((*ptr == '&' && *(ptr + 1) == '&') || 
                (*ptr == '|' && *(ptr + 1) == '|')) {
                
                // Сохраняем текущий токен, если есть
                if (token_pos > 0) {
                    current_token[token_pos] = '\0';
                    if (temp_count < MAX_WORDS - 1) {
                        temp_words[temp_count++] = strdup(current_token);
                    }
                    token_pos = 0;
                }
                
                // Сохраняем двойной символ как один токен
                if (temp_count < MAX_WORDS - 1) {
                    char double_token[3] = {*ptr, *(ptr + 1), '\0'};
                    temp_words[temp_count++] = strdup(double_token);
                }
                
                ptr += 2; // Пропускаем два символа
                continue;
            }
            
            // Проверка на перенаправления stderr (2>, 2>>, 2>&1)
            if ((*ptr == '2' && *(ptr + 1) == '>' && *(ptr + 2) == '&' && *(ptr + 3) == '1')) {
                // Обработка 2>&1
                if (token_pos > 0) {
                    current_token[token_pos] = '\0';
                    if (temp_count < MAX_WORDS - 1) {
                        temp_words[temp_count++] = strdup(current_token);
                    }
                    token_pos = 0;
                }
                
                if (temp_count < MAX_WORDS - 1) {
                    temp_words[temp_count++] = strdup("2>&1");
                }
                
                ptr += 4;
                continue;
            } else if ((*ptr == '2' && *(ptr + 1) == '>' && *(ptr + 2) == '>')) {
                // Обработка 2>>
                if (token_pos > 0) {
                    current_token[token_pos] = '\0';
                    if (temp_count < MAX_WORDS - 1) {
                        temp_words[temp_count++] = strdup(current_token);
                    }
                    token_pos = 0;
                }
                
                if (temp_count < MAX_WORDS - 1) {
                    temp_words[temp_count++] = strdup("2>>");
                }
                
                ptr += 3;
                continue;
            } else if ((*ptr == '2' && *(ptr + 1) == '>')) {
                // Обработка 2>
                if (token_pos > 0) {
                    current_token[token_pos] = '\0';
                    if (temp_count < MAX_WORDS - 1) {
                        temp_words[temp_count++] = strdup(current_token);
                    }
                    token_pos = 0;
                }
                
                if (temp_count < MAX_WORDS - 1) {
                    temp_words[temp_count++] = strdup("2>");
                }
                
                ptr += 2;
                continue;
            }

            // Проверка на одиночные символы перенаправления и разделители
            if (*ptr == '>' || *ptr == '<' || *ptr == '|' || *ptr == '&' || *ptr == ';') {
                // Сохраняем текущий токен, если есть
                if (token_pos > 0) {
                    current_token[token_pos] = '\0';
                    if (temp_count < MAX_WORDS - 1) {
                        temp_words[temp_count++] = strdup(current_token);
                    }
                    token_pos = 0;
                }
                // Сохраняем специальный символ как отдельный токен
                if (temp_count < MAX_WORDS - 1) {
                    char special_token[2] = {*ptr, '\0'};
                    temp_words[temp_count++] = strdup(special_token);
                }
                ptr++;
            } else if (strchr(DELIMITERS, *ptr)) {
                // Сохраняем текущий токен, если есть
                if (token_pos > 0) {
                    current_token[token_pos] = '\0';
                    if (temp_count < MAX_WORDS - 1) {
                        temp_words[temp_count++] = strdup(current_token);
                    }
                    token_pos = 0;
                }
                ptr++;
            } else {
                // Обычный символ - добавляем в текущий токен
                if (token_pos < MAX_INPUT_LENGTH - 1) {
                    current_token[token_pos++] = *ptr;
                }
                ptr++;
            }
        }
    }

    // Последний токен
    if (token_pos > 0) {
        current_token[token_pos] = '\0';
        if (temp_count < MAX_WORDS - 1) {
            temp_words[temp_count++] = strdup(current_token);
        }
    }

    free(line);

    // Обработка фонового режима - ТОЛЬКО одиночный & в конце
    if (temp_count > 0) {
        // Проверяем, что & не в середине как одиночный символ
        for (int i = 0; i < temp_count; i++) {
            if (strcmp(temp_words[i], "&") == 0) {
                if (i == temp_count - 1) {
                    // & в конце - это фоновый режим
                    cmd->fonius = 1;
                    free(temp_words[i]);
                    // Сдвигаем остальные элементы
                    for (int j = i; j < temp_count - 1; j++) {
                        temp_words[j] = temp_words[j + 1];
                    }
                    temp_count--;
                } else {
                    // & в середине - это ошибка (если это не часть &&)
                    fprintf(stderr, "Error: '&' must be at the end of command\n");
                    // Освобождаем временную память
                    for (int j = 0; j < temp_count; j++) {
                        free(temp_words[j]);
                    }
                    free(temp_words);
                    free(cmd);
                    return NULL;
                }
                break;
            }
        }
    }

    // Обработка перенаправлений и конвейеров
    process_redirections_and_pipes(cmd, temp_words, &temp_count);

    // Копируем токены в структуру команды
    cmd->words = malloc((temp_count + 1) * sizeof(char*));
    if (cmd->words == NULL) {
        perror("malloc");
        for (int i = 0; i < temp_count; i++) {
            free(temp_words[i]);
        }
        free(temp_words);
        free(cmd);
        return NULL;
    }

    for (int i = 0; i < temp_count; i++) {
        cmd->words[i] = temp_words[i];
    }
    cmd->words[temp_count] = NULL;
    cmd->word_num = temp_count;

    free(temp_words);
    return cmd;
}

// Вспомогательная функция для удаления токенов
void remove_words(char **words, int *count, int start, int num) {
    if (start < 0 || start >= *count || num <= 0) return;
    
    // Освобождаем удаляемые токены
    for (int i = start; i < start + num && i < *count; i++) {
        free(words[i]);
    }
    
    // Сдвигаем оставшиеся токены
    for (int i = start; i < *count - num; i++) {
        words[i] = words[i + num];
    }
    
    *count -= num;
}

// НОВАЯ ФУНКЦИЯ: копирование информации о перенаправлениях
void copy_redirections(command_t *dest, const command_t *src) {
    if (src->input_file) dest->input_file = strdup(src->input_file);
    if (src->output_file) dest->output_file = strdup(src->output_file);
    if (src->error_file) dest->error_file = strdup(src->error_file);
    dest->append_output = src->append_output;
    dest->append_error = src->append_error;
    dest->merge_output = src->merge_output;
    dest->fonius = src->fonius;
}

// Парсинг ввода с разделителями команд
command_sequence_t *parse_input_with_separators(const char *input) {
    if (input == NULL || strlen(input) == 0) {
        return NULL;
    }

    // Сначала парсим всю строку как обычно
    command_t *full_cmd = parse_input(input);
    if (full_cmd == NULL) {
        return NULL;
    }

    command_sequence_t *seq = malloc(sizeof(command_sequence_t));
    if (seq == NULL) {
        perror("malloc");
        free_command(full_cmd);
        return NULL;
    }

    seq->commands = NULL;
    seq->command_count = 0;
    seq->separators = NULL;

    // Временные массивы
    command_t **temp_commands = malloc(MAX_WORDS * sizeof(command_t *));
    int *temp_separators = malloc(MAX_WORDS * sizeof(int));
    
    if (temp_commands == NULL || temp_separators == NULL) {
        perror("malloc");
        free_command(full_cmd);
        free(seq);
        free(temp_commands);
        free(temp_separators);
        return NULL;
    }

    int cmd_start = 0;
    int seq_index = 0;

    for (int i = 0; i <= full_cmd->word_num; i++) {
        // Проверяем на разделитель или конец
        if (i == full_cmd->word_num || is_command_separator(full_cmd->words[i])) {
            // Создаем команду из сегмента
            if (i > cmd_start) {
                command_t *sub_cmd = malloc(sizeof(command_t));
                if (sub_cmd == NULL) {
                    perror("malloc");
                    // Освобождаем уже созданные команды
                    for (int j = 0; j < seq_index; j++) {
                        free_command(temp_commands[j]);
                    }
                    free_command(full_cmd);
                    free(seq);
                    free(temp_commands);
                    free(temp_separators);
                    return NULL;
                }

                // Инициализируем подкоманду
                sub_cmd->words = NULL;
                sub_cmd->word_num = 0;
                sub_cmd->fonius = 0;
                sub_cmd->input_file = NULL;
                sub_cmd->output_file = NULL;
                sub_cmd->error_file = NULL;
                sub_cmd->append_output = 0;
                sub_cmd->append_error = 0;
                sub_cmd->merge_output = 0;
                sub_cmd->pipeline = NULL;
                sub_cmd->pipeline_count = 0;

                // Копируем слова
                sub_cmd->word_num = i - cmd_start;
                sub_cmd->words = malloc((sub_cmd->word_num + 1) * sizeof(char *));
                if (sub_cmd->words == NULL) {
                    perror("malloc");
                    free(sub_cmd);
                    for (int j = 0; j < seq_index; j++) {
                        free_command(temp_commands[j]);
                    }
                    free_command(full_cmd);
                    free(seq);
                    free(temp_commands);
                    free(temp_separators);
                    return NULL;
                }

                for (int j = cmd_start; j < i; j++) {
                    sub_cmd->words[j - cmd_start] = strdup(full_cmd->words[j]);
                    if (sub_cmd->words[j - cmd_start] == NULL) {
                        perror("strdup");
                        for (int k = cmd_start; k < j; k++) {
                            free(sub_cmd->words[k - cmd_start]);
                        }
                        free(sub_cmd->words);
                        free(sub_cmd);
                        for (int k = 0; k < seq_index; k++) {
                            free_command(temp_commands[k]);
                        }
                        free_command(full_cmd);
                        free(seq);
                        free(temp_commands);
                        free(temp_separators);
                        return NULL;
                    }
                }
                sub_cmd->words[sub_cmd->word_num] = NULL;

                // ВАЖНО: Копируем перенаправления из оригинальной команды
                // Это позволяет командам в последовательности наследовать перенаправления
                copy_redirections(sub_cmd, full_cmd);

                temp_commands[seq_index] = sub_cmd;

                // Сохраняем разделитель (если есть)
                if (i < full_cmd->word_num) {
                    temp_separators[seq_index] = get_separator_type(full_cmd->words[i]);
                }
                
                seq_index++;
            }
            cmd_start = i + 1;
        }
    }

    // Копируем в итоговую структуру
    seq->command_count = seq_index;
    seq->commands = malloc(seq_index * sizeof(command_t *));
    seq->separators = malloc((seq_index > 0 ? seq_index - 1 : 0) * sizeof(int));
    
    if (seq->commands == NULL || (seq_index > 0 && seq->separators == NULL)) {
        perror("malloc");
        for (int i = 0; i < seq_index; i++) {
            free_command(temp_commands[i]);
        }
        free_command(full_cmd);
        free(seq);
        free(temp_commands);
        free(temp_separators);
        return NULL;
    }

    for (int i = 0; i < seq_index; i++) {
        seq->commands[i] = temp_commands[i];
        if (i < seq_index - 1) {
            seq->separators[i] = temp_separators[i];
        }
    }

    free(temp_commands);
    free(temp_separators);
    free_command(full_cmd);

    return seq;
}

// Освобождение последовательности команд
void free_command_sequence(command_sequence_t *seq) {
    if (seq == NULL) return;
    
    for (int i = 0; i < seq->command_count; i++) {
        free_command(seq->commands[i]);
    }
    free(seq->commands);
    free(seq->separators);
    free(seq);
}

// Отладка последовательности команд
void print_command_sequence(const command_sequence_t *seq) {
    if (seq == NULL) {
        printf("Command sequence: NULL\n");
        return;
    }
    
    printf("Command sequence (%d commands):\n", seq->command_count);
    for (int i = 0; i < seq->command_count; i++) {
        printf("  Command %d: ", i + 1);
        for (int j = 0; j < seq->commands[i]->word_num; j++) {
            printf("[%s] ", seq->commands[i]->words[j]);
        }
        printf("\n");
        
        // Выводим информацию о перенаправлениях
        if (seq->commands[i]->input_file) 
            printf("    Input: %s\n", seq->commands[i]->input_file);
        if (seq->commands[i]->output_file) 
            printf("    Output: %s (%s)\n", seq->commands[i]->output_file, 
                   seq->commands[i]->append_output ? "append" : "trunc");
        if (seq->commands[i]->error_file) 
            printf("    Error: %s (%s)\n", seq->commands[i]->error_file, 
                   seq->commands[i]->append_error ? "append" : "trunc");
        if (seq->commands[i]->merge_output) 
            printf("    Merge: stderr to stdout\n");
        
        if (i < seq->command_count - 1) {
            const char *sep_name;
            switch (seq->separators[i]) {
                case 0: sep_name = ";"; break;
                case 1: sep_name = "&&"; break;
                case 2: sep_name = "||"; break;
                default: sep_name = "?"; break;
            }
            printf("  Separator: %s\n", sep_name);
        }
    }
}

void process_redirections_and_pipes(command_t *cmd, char **words, int *word_num) {
    int i = 0;
    
    while (i < *word_num) {
        // Пропускаем пустые слова и разделители команд
        if (words[i] == NULL || strlen(words[i]) == 0 || is_command_separator(words[i])) {
            i++;
            continue;
        }
        
        int tokens_to_remove = 0;
        char *filename = NULL;
        
        // Проверяем на перенаправления
        if (strcmp(words[i], ">") == 0) {
            // Обычное перенаправление вывода
            if (i + 1 >= *word_num) {
                fprintf(stderr, "Error: expected filename after '>'\n");
                i++;
                continue;
            }
            
            // Проверяем, что следующий токен не является специальным символом
            if (is_redirection_char(words[i+1]) || is_command_separator(words[i+1]) || 
                strcmp(words[i+1], "|") == 0 || strcmp(words[i+1], "&") == 0) {
                fprintf(stderr, "Error: filename cannot be special character '%s'\n", words[i+1]);
                i++;
                continue;
            }
            
            if (cmd->output_file != NULL) {
                fprintf(stderr, "Warning: multiple output redirections, using last one\n");
                free(cmd->output_file);
            }
            
            filename = strdup(words[i + 1]);
            if (filename == NULL) {
                perror("strdup");
                i++;
                continue;
            }
                        
            cmd->output_file = filename;
            cmd->append_output = 0;
            tokens_to_remove = 2;
            
        } else if (strcmp(words[i], ">>") == 0) {
            // Перенаправление вывода с дополнением
            if (i + 1 >= *word_num) {
                fprintf(stderr, "Error: expected filename after '>>'\n");
                i++;
                continue;
            }
            
            // Проверяем, что следующий токен не является специальным символом
            if (is_redirection_char(words[i+1]) || is_command_separator(words[i+1]) || 
                strcmp(words[i+1], "|") == 0 || strcmp(words[i+1], "&") == 0) {
                fprintf(stderr, "Error: filename cannot be special character '%s'\n", words[i+1]);
                i++;
                continue;
            }
            
            if (cmd->output_file != NULL) {
                fprintf(stderr, "Warning: multiple output redirections, using last one\n");
                free(cmd->output_file);
            }
            
            filename = strdup(words[i + 1]);
            if (filename == NULL) {
                perror("strdup");
                i++;
                continue;
            }
            
            cmd->output_file = filename;
            cmd->append_output = 1;
            tokens_to_remove = 2;
            
        } else if (strcmp(words[i], "<") == 0) {
            // Перенаправление ввода
            if (i + 1 >= *word_num) {
                fprintf(stderr, "Error: expected filename after '<'\n");
                i++;
                continue;
            }
            
            // Проверяем, что следующий токен не является специальным символом
            if (is_redirection_char(words[i+1]) || is_command_separator(words[i+1]) || 
                strcmp(words[i+1], "|") == 0 || strcmp(words[i+1], "&") == 0) {
                fprintf(stderr, "Error: filename cannot be special character '%s'\n", words[i+1]);
                i++;
                continue;
            }
            
            if (cmd->input_file != NULL) {
                fprintf(stderr, "Warning: multiple input redirections, using last one\n");
                free(cmd->input_file);
            }
            
            filename = strdup(words[i + 1]);
            if (filename == NULL) {
                perror("strdup");
                i++;
                continue;
            }
            
            cmd->input_file = filename;
            tokens_to_remove = 2;
            
        } else if (strcmp(words[i], "2>") == 0) {
            // Перенаправление stderr
            if (i + 1 >= *word_num) {
                fprintf(stderr, "Error: expected filename after '2>'\n");
                i++;
                continue;
            }
            
            // Проверяем, что следующий токен не является специальным символом
            if (is_redirection_char(words[i+1]) || is_command_separator(words[i+1]) || 
                strcmp(words[i+1], "|") == 0 || strcmp(words[i+1], "&") == 0) {
                fprintf(stderr, "Error: filename cannot be special character '%s'\n", words[i+1]);
                i++;
                continue;
            }
            
            if (cmd->error_file != NULL) {
                fprintf(stderr, "Warning: multiple stderr redirections, using last one\n");
                free(cmd->error_file);
            }
            
            filename = strdup(words[i + 1]);
            if (filename == NULL) {
                perror("strdup");
                i++;
                continue;
            }
            
            cmd->error_file = filename;
            cmd->append_error = 0;
            tokens_to_remove = 2;
            
        } else if (strcmp(words[i], "2>>") == 0) {
            // Перенаправление stderr с дополнением
            if (i + 1 >= *word_num) {
                fprintf(stderr, "Error: expected filename after '2>>'\n");
                i++;
                continue;
            }
            
            // Проверяем, что следующий токен не является специальным символом
            if (is_redirection_char(words[i+1]) || is_command_separator(words[i+1]) || 
                strcmp(words[i+1], "|") == 0 || strcmp(words[i+1], "&") == 0) {
                fprintf(stderr, "Error: filename cannot be special character '%s'\n", words[i+1]);
                i++;
                continue;
            }
            
            if (cmd->error_file != NULL) {
                fprintf(stderr, "Warning: multiple stderr redirections, using last one\n");
                free(cmd->error_file);
            }
            
            filename = strdup(words[i + 1]);
            if (filename == NULL) {
                perror("strdup");
                i++;
                continue;
            }
            
            cmd->error_file = filename;
            cmd->append_error = 1;
            tokens_to_remove = 2;
            
        } else if (strcmp(words[i], "2>&1") == 0) {
            // Объединение stderr с stdout
            if (cmd->merge_output) {
                fprintf(stderr, "Warning: multiple output merges\n");
            }
            
            cmd->merge_output = 1;
            tokens_to_remove = 1;
        }
        
        // Удаляем обработанные токены
        if (tokens_to_remove > 0) {
            remove_words(words, word_num, i, tokens_to_remove);
            // Не увеличиваем i, так как массив сдвинулся
        } else {
            i++;
        }
    }
}

// Функция освобождения памяти
void free_command(command_t *cmd) {
    if (cmd == NULL) return;

    // Освобождаем токены
    for (int i = 0; i < cmd->word_num; i++) {
        free(cmd->words[i]);
    }
    free(cmd->words);
    
    // Освобождаем имена файлов
    free(cmd->input_file);
    free(cmd->output_file);
    free(cmd->error_file);

    // Освобождаем конвейер (если есть)
    if (cmd->pipeline != NULL) {
        for (int i = 0; i < cmd->pipeline_count; i++) {
            for (int j = 0; cmd->pipeline[i][j] != NULL; j++) {
                free(cmd->pipeline[i][j]);
            }
            free(cmd->pipeline[i]);
        }
        free(cmd->pipeline);
    }
    
    free(cmd);
}

// Дополнительная функция для отладки
void print_command(const command_t *cmd) {
    if (cmd == NULL) {
        printf("Command: NULL\n");
        return;
    }
    
    printf("Command words: ");
    for (int i = 0; i < cmd->word_num; i++) {
        printf("[%s] ", cmd->words[i]);
    }
    printf("\n");
    
    printf("fonius: %s\n", cmd->fonius ? "yes" : "no");
    printf("Input file: %s\n", cmd->input_file ? cmd->input_file : "NULL");
    printf("Output file: %s\n", cmd->output_file ? cmd->output_file : "NULL");
    printf("Error file: %s\n", cmd->error_file ? cmd->error_file : "NULL");  
    printf("Append output: %s\n", cmd->append_output ? "yes" : "no");
    printf("Append error: %s\n", cmd->append_error ? "yes" : "no");          
    printf("Merge output: %s\n", cmd->merge_output ? "yes" : "no");          
    printf("Pipeline count: %d\n", cmd->pipeline_count);
}