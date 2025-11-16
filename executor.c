#include "shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>

// Функция для применения перенаправлений
int apply_redirections(command_t *cmd) {
    // Перенаправление ввода (stdin)
    if (cmd->input_file != NULL) {
        int fd = open(cmd->input_file, O_RDONLY);
        if (fd < 0) {
            perror("open input");
            return -1;
        }
        
        if (dup2(fd, STDIN_FILENO) < 0) {
            perror("dup2 stdin");
            close(fd);
            return -1;
        }
        close(fd);
    }
    
    // Перенаправление вывода (stdout)
    if (cmd->output_file != NULL) {
        int flags = O_WRONLY | O_CREAT;
        if (cmd->append_output) {
            flags |= O_APPEND;
        } else {
            flags |= O_TRUNC;
        }
        
        int fd = open(cmd->output_file, flags, 0644);
        if (fd < 0) {
            perror("open output");
            return -1;
        }
        
        if (dup2(fd, STDOUT_FILENO) < 0) {
            perror("dup2 stdout");
            close(fd);
            return -1;
        }
        close(fd);
    }
    
    // Перенаправление ошибок (stderr)
    if (cmd->error_file != NULL) {
        int flags = O_WRONLY | O_CREAT;
        if (cmd->append_error) {
            flags |= O_APPEND;
        } else {
            flags |= O_TRUNC;
        }
        
        int fd = open(cmd->error_file, flags, 0644);
        if (fd < 0) {
            perror("open error");
            return -1;
        }
        
        if (dup2(fd, STDERR_FILENO) < 0) {
            perror("dup2 stderr");
            close(fd);
            return -1;
        }
        close(fd);
    }
    
    // Объединение stderr с stdout
    if (cmd->merge_output) {
        if (dup2(STDOUT_FILENO, STDERR_FILENO) < 0) {
            perror("dup2 merge");
            return -1;
        }
    }
    
    return 0;
}

char *get_full_path(const char *command) {
    if (command == NULL || command[0] == '\0') {
        return NULL;
    }

    // Если команда уже содержит абсолютный путь или относительный путь
    if (command[0] == '/' || (command[0] == '.' && (command[1] == '/' || 
        (command[1] == '.' && command[2] == '/')))) {
        if (access(command, X_OK) == 0) {
            return strdup(command);
        }
        return NULL;
    }

    char *path_env = getenv("PATH");
    if (path_env == NULL) {
        return NULL;
    }

    char *path_copy = strdup(path_env);
    if (path_copy == NULL) {
        perror("strdup");
        return NULL;
    }

    char *dir = strtok(path_copy, ":");
    char *found_path = NULL;

    while (dir != NULL) {
        // Проверка длины пути
        size_t dir_len = strlen(dir);
        size_t cmd_len = strlen(command);
        
        if (dir_len + cmd_len + 2 > MAX_PATH_LENGTH) {
            dir = strtok(NULL, ":");
            continue;
        }

        char full_path[MAX_PATH_LENGTH];
        int written = snprintf(full_path, sizeof(full_path), "%s/%s", dir, command);
        
        if (written < 0 || written >= (int)sizeof(full_path)) {
            dir = strtok(NULL, ":");
            continue;
        }

        if (access(full_path, X_OK) == 0) {
            found_path = strdup(full_path);
            break;
        }

        dir = strtok(NULL, ":");
    }

    free(path_copy);
    return found_path;
}

int execute_external(command_t *cmd) {
    // Находим полный путь к команде
    char *full_path = get_full_path(cmd->words[0]);
    if (full_path == NULL) {
        fprintf(stderr, "%s: command not found\n", cmd->words[0]);
        return 127;
    }

    pid_t pid = fork();
    
    if (pid == -1) {
        perror("fork");
        free(full_path);
        return 1;
    } else if (pid == 0) {
        // Дочерний процесс
        
        // ПРИМЕНЯЕМ ПЕРЕНАПРАВЛЕНИЯ
        if (apply_redirections(cmd) < 0) {
            fprintf(stderr, "Error: failed to apply redirections\n");
            free(full_path);
            exit(1);
        }
        
        execv(full_path, cmd->words);
        
        // Если execv вернул управление - произошла ошибка
        perror("execv");
        free(full_path);
        exit(1);
    } else {
        // Родительский процесс
        free(full_path);
        
        if (!cmd->fonius) {
            int status;
            waitpid(pid, &status, 0);
            return WEXITSTATUS(status);
        } else {
            printf("[%d] Started in fonius\n", pid);
            return 0;
        }
    }
}

// Вспомогательная функция для разделения конвейера на команды
command_t **split_pipeline(command_t *cmd, int *cmd_count) {
    // Считаем количество команд в конвейере
    *cmd_count = 1;
    for (int i = 0; i < cmd->word_num; i++) {
        if (strcmp(cmd->words[i], "|") == 0) {
            (*cmd_count)++;
        }
    }

    // Выделяем память для массива команд
    command_t **commands = malloc(*cmd_count * sizeof(command_t *));
    if (commands == NULL) {
        perror("malloc");
        return NULL;
    }

    int start = 0;
    int cmd_index = 0;

    for (int i = 0; i <= cmd->word_num; i++) {
        // Нашли разделитель или конец массива
        if (i == cmd->word_num || strcmp(cmd->words[i], "|") == 0) {
            // Создаем новую команду
            command_t *new_cmd = malloc(sizeof(command_t));
            if (new_cmd == NULL) {
                perror("malloc");
                // Освобождаем уже созданные команды
                for (int j = 0; j < cmd_index; j++) {
                    free_command(commands[j]);
                }
                free(commands);
                return NULL;
            }

            // Инициализируем команду
            new_cmd->words = NULL;
            new_cmd->word_num = i - start;
            new_cmd->fonius = 0;
            new_cmd->input_file = NULL;
            new_cmd->output_file = NULL;
            new_cmd->error_file = NULL;
            new_cmd->append_output = 0;
            new_cmd->append_error = 0;
            new_cmd->merge_output = 0;
            new_cmd->pipeline = NULL;
            new_cmd->pipeline_count = 0;

            // Копируем токены для этой команды
            if (new_cmd->word_num > 0) {
                new_cmd->words = malloc((new_cmd->word_num + 1) * sizeof(char *));
                if (new_cmd->words == NULL) {
                    perror("malloc");
                    free(new_cmd);
                    for (int j = 0; j < cmd_index; j++) {
                        free_command(commands[j]);
                    }
                    free(commands);
                    return NULL;
                }

                for (int j = 0; j < new_cmd->word_num; j++) {
                    new_cmd->words[j] = strdup(cmd->words[start + j]);
                    if (new_cmd->words[j] == NULL) {
                        perror("strdup");
                        for (int k = 0; k < j; k++) {
                            free(new_cmd->words[k]);
                        }
                        free(new_cmd->words);
                        free(new_cmd);
                        for (int k = 0; k < cmd_index; k++) {
                            free_command(commands[k]);
                        }
                        free(commands);
                        return NULL;
                    }
                }
                new_cmd->words[new_cmd->word_num] = NULL;
            }

            commands[cmd_index++] = new_cmd;
            start = i + 1;  // Пропускаем символ "|"
        }
    }

    return commands;
}

int execute_pipeline(command_t *cmd) {
    int cmd_count;
    command_t **commands = split_pipeline(cmd, &cmd_count);
    
    if (commands == NULL) {
        return 1;
    }

    if (cmd_count < 2) {
        fprintf(stderr, "Invalid pipeline: need at least 2 commands\n");
        for (int i = 0; i < cmd_count; i++) {
            free_command(commands[i]);
        }
        free(commands);
        return 1;
    }

    // Создаем массив pipe'ов
    int (*pipefds)[2] = malloc((cmd_count - 1) * sizeof(int[2]));
    if (pipefds == NULL) {
        perror("malloc");
        for (int i = 0; i < cmd_count; i++) {
            free_command(commands[i]);
        }
        free(commands);
        return 1;
    }

    // Создаем все pipe'ы
    for (int i = 0; i < cmd_count - 1; i++) {
        if (pipe(pipefds[i]) == -1) {
            perror("pipe");
            // Закрываем уже созданные pipe'ы
            for (int j = 0; j < i; j++) {
                close(pipefds[j][0]);
                close(pipefds[j][1]);
            }
            free(pipefds);
            for (int j = 0; j < cmd_count; j++) {
                free_command(commands[j]);
            }
            free(commands);
            return 1;
        }
    }

    // Массив для хранения PID'ов всех процессов
    pid_t *pids = malloc(cmd_count * sizeof(pid_t));
    if (pids == NULL) {
        perror("malloc");
        for (int i = 0; i < cmd_count - 1; i++) {
            close(pipefds[i][0]);
            close(pipefds[i][1]);
        }
        free(pipefds);
        for (int i = 0; i < cmd_count; i++) {
            free_command(commands[i]);
        }
        free(commands);
        return 1;
    }

    // Создаем процессы для каждой команды
    for (int i = 0; i < cmd_count; i++) {
        pids[i] = fork();
        
        if (pids[i] == -1) {
            perror("fork");
            // Убиваем уже созданные процессы
            for (int j = 0; j < i; j++) {
                kill(pids[j], SIGTERM);
            }
            // Закрываем pipe'ы
            for (int j = 0; j < cmd_count - 1; j++) {
                close(pipefds[j][0]);
                close(pipefds[j][1]);
            }
            free(pipefds);
            free(pids);
            for (int j = 0; j < cmd_count; j++) {
                free_command(commands[j]);
            }
            free(commands);
            return 1;
        }

        if (pids[i] == 0) {
            // Дочерний процесс
            
            // Подключаем вход
            if (i > 0) {
                dup2(pipefds[i-1][0], STDIN_FILENO);
                close(pipefds[i-1][0]);
            }
            
            // Подключаем выход
            if (i < cmd_count - 1) {
                dup2(pipefds[i][1], STDOUT_FILENO);
                close(pipefds[i][1]);
            }
            
            // Закрываем все pipe'ы в дочернем процессе
            for (int j = 0; j < cmd_count - 1; j++) {
                if (j != i - 1) close(pipefds[j][0]);
                if (j != i) close(pipefds[j][1]);
            }
            
            // ПРИМЕНЯЕМ ПЕРЕНАПРАВЛЕНИЯ ДЛЯ ЭТОЙ КОМАНДЫ
            if (apply_redirections(commands[i]) < 0) {
                fprintf(stderr, "Error: failed to apply redirections for command %d\n", i);
                exit(1);
            }
            
            // Выполняем команду
            int result = execute_command(commands[i]);
            
            // Освобождаем память перед выходом
            for (int j = 0; j < cmd_count; j++) {
                if (j != i) free_command(commands[j]);
            }
            free(commands);
            free(pipefds);
            free(pids);
            
            exit(result);
        }
    }

    // Родительский процесс - закрываем все pipe'ы
    for (int i = 0; i < cmd_count - 1; i++) {
        close(pipefds[i][0]);
        close(pipefds[i][1]);
    }

    // Ожидаем завершения всех процессов
    int last_status = 0;
    for (int i = 0; i < cmd_count; i++) {
        int status;
        waitpid(pids[i], &status, 0);
        if (i == cmd_count - 1) {  // Сохраняем статус последней команды
            last_status = WEXITSTATUS(status);
        }
    }

    // Освобождаем память
    free(pipefds);
    free(pids);
    for (int i = 0; i < cmd_count; i++) {
        free_command(commands[i]);
    }
    free(commands);

    return last_status;
}

// НОВАЯ ФУНКЦИЯ: Выполнение последовательности команд с учетом логики && и ||
int execute_command_sequence(command_sequence_t *seq) {
    if (seq == NULL || seq->command_count == 0) {
        return 0;
    }

    // Если только одна команда - выполняем как обычно
    if (seq->command_count == 1) {
        return execute_command(seq->commands[0]);
    }

    int last_status = 0;
    int should_execute_next = 1; // Флаг: нужно ли выполнять следующую команду

    for (int i = 0; i < seq->command_count; i++) {
        // Выполняем текущую команду только если нужно
        int current_status = 0;
        if (should_execute_next) {
            current_status = execute_command(seq->commands[i]);
        } else {
            // Пропускаем выполнение, но сохраняем статус последней выполненной команды
            current_status = last_status;
        }
        
        last_status = current_status;
        
        // Определяем, нужно ли выполнять следующую команду
        if (i < seq->command_count - 1) {
            int separator = seq->separators[i];
            
            switch (separator) {
                case 0: // ; - ВСЕГДА выполняем следующую команду
                    should_execute_next = 1;
                    break;
                    
                case 1: // && - выполняем следующую только если УСПЕХ (status == 0)
                    should_execute_next = (current_status == 0);
                    break;
                    
                case 2: // || - выполняем следующую только если ОШИБКА (status != 0)
                    should_execute_next = (current_status != 0);
                    break;
                    
                default:
                    fprintf(stderr, "Unknown separator type: %d\n", separator);
                    should_execute_next = 1;
                    break;
            }
        }
    }

    return last_status;
}

int execute_command(command_t *cmd) {
    if (cmd == NULL || cmd->word_num == 0) {
        return 0;
    }

    // Проверяем на наличие разделителей команд в самой команде
    int has_separators = 0;
    for (int i = 0; i < cmd->word_num; i++) {
        if (is_command_separator(cmd->words[i])) {
            has_separators = 1;
            break;
        }
    }
    
    if (has_separators) {
        // Перестраиваем исходную строку для парсинга последовательности
        char *reconstructed_input = malloc(MAX_INPUT_LENGTH);
        if (reconstructed_input == NULL) {
            perror("malloc");
            return 1;
        }
        
        reconstructed_input[0] = '\0';
        
        // Собираем исходную строку из слов команды
        for (int j = 0; j < cmd->word_num; j++) {
            if (j > 0) {
                strcat(reconstructed_input, " ");
            }
            strcat(reconstructed_input, cmd->words[j]);
        }
        
        // Парсим как последовательность команд
        command_sequence_t *seq = parse_input_with_separators(reconstructed_input);
        free(reconstructed_input);
        
        if (seq != NULL) {
            int result = execute_command_sequence(seq);
            free_command_sequence(seq);
            return result;
        } else {
            return 1; // Ошибка парсинга
        }
    }

    // Проверяем на конвейер
    for (int i = 0; i < cmd->word_num; i++) {
        if (strcmp(cmd->words[i], "|") == 0) {
            return execute_pipeline(cmd);
        }
    }

    // Проверяем встроенные команды
    int builtin_result = execute_bash_cmd(cmd->words);
    if (builtin_result != -1) {
        return builtin_result;
    }

    // Внешние команды
    return execute_external(cmd);
}