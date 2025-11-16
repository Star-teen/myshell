#include "shell.h"
#include "shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <termios.h>
#include <ctype.h>

history_t *global_history = NULL;

void check_child(int sig) {
    int status;
    pid_t pid;
    
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        printf("[%d] Finished with status %d\n", pid, WEXITSTATUS(status));
    }
}

char* print_dir() {
    long size = pathconf(".", _PC_PATH_MAX);
    if (size == -1) {
        size = 4096;
    }

    char *cwd = malloc((size_t)size);
    if (cwd == NULL) {
        perror("malloc");
        exit(2);
    }

    cwd = getcwd(cwd, (size_t)size);
    if (cwd == NULL) {
        perror("getcwd");
        free(cwd);
        exit(2);
    }

    return cwd;
}

char *read_line(void) {
    char *line = NULL;
    size_t linesize = 0;
    char* dir = print_dir();

    printf("%s> ", dir);
    fflush(stdout);
    free(dir);
    
    ssize_t num_chars_read = getline(&line, &linesize, stdin);
    
    if (num_chars_read <= 0) {
        free(line);
        return NULL;
    }
    
    if (line[num_chars_read - 1] == '\n') {
        line[num_chars_read - 1] = '\0';
    }
    
    return line;
}

// Новая функция чтения строки с поддержкой истории
char *read_line_with_history(history_t *hist) {
    static int terminal_initialized = 0;
    
    if (!terminal_initialized) {
        if (setup_terminal() == -1) {
            fprintf(stderr, "Warning: Failed to setup terminal, using basic input\n");
            // Возвращаемся к базовой реализации
            char *line = read_line();
            if (hist && line && strlen(line) > 0) {
                add_to_history(hist, line);
            }
            return line;
        }
        terminal_initialized = 1;
    }
    
    char *line = malloc(MAX_INPUT_LENGTH);
    if (!line) {
        perror("malloc");
        return NULL;
    }
    
    int pos = 0;
    int hist_index = -1;  // -1 = новая команда
    line[0] = '\0';
    
    char *dir = print_dir();
    printf("%s> ", dir);
    free(dir);
    fflush(stdout);
    
    while (1) {
        char c = getchar();
        
        if (c == '\n') {  // Enter
            break;
        } else if (c == '\x1b') {  // Escape sequence (стрелки)
            char c2 = getchar();
            if (c2 == '[') {
                char c3 = getchar();
                
                if (c3 == 'A' && hist) {  // Стрелка вверх
                    if (hist_index < hist->count - 1) {
                        hist_index++;
                        const char *prev_cmd = get_history_command(hist, hist_index);
                        if (prev_cmd) {
                            clear_current_line(pos);
                            strcpy(line, prev_cmd);
                            pos = strlen(line);
                            printf("%s", line);
                            fflush(stdout);
                        }
                    }
                } else if (c3 == 'B' && hist) {  // Стрелка вниз
                    if (hist_index > 0) {
                        hist_index--;
                        const char *prev_cmd = get_history_command(hist, hist_index);
                        clear_current_line(pos);
                        if (prev_cmd) {
                            strcpy(line, prev_cmd);
                            pos = strlen(line);
                            printf("%s", line);
                        } else {
                            line[0] = '\0';
                            pos = 0;
                        }
                        fflush(stdout);
                    } else if (hist_index == 0) {
                        hist_index = -1;
                        clear_current_line(pos);
                        line[0] = '\0';
                        pos = 0;
                        fflush(stdout);
                    }
                }
            }
        } else if (c == 127 || c == '\b') {  // Backspace
            if (pos > 0) {
                pos--;
                line[pos] = '\0';
                printf("\b \b");
                fflush(stdout);
            }
        } else if (c == '\t') {  // Tab - игнорируем
            continue;
        } else if (isprint(c)) {  // Печатные символы
            if (pos < MAX_INPUT_LENGTH - 1) {
                line[pos++] = c;
                line[pos] = '\0';
                putchar(c);
                fflush(stdout);
                
                // Сбрасываем навигацию по истории при вводе
                if (hist_index != -1) {
                    hist_index = -1;
                }
            }
        }
        // Игнорируем другие управляющие символы
    }
    
    printf("\n");
    return line;
}

int main(void) {
    char *input;
    
    // Инициализация истории команд
    history_t *history = init_history(MAX_HISTORY_SIZE);
    if (!history) {
        fprintf(stderr, "Warning: Failed to initialize command history\n");
    } else {
        // Загружаем историю из файла
        load_history(history, HISTORY_FILE);
        printf("Command history loaded (%d commands)\n", history->count);
    }
    
    signal(SIGCHLD, check_child);
    
    printf("Shell R v7.2 with Command History\n");
    printf("Type 'exit' to quit. Use Up/Down arrows for history.\n");
    
    while (1) {
        if (history) {
            input = read_line_with_history(history);
        } else {
            input = read_line();  // Fallback to basic input
        }
        
        if (input == NULL) {
            break;  // EOF или ошибка
        }
        
        if (strlen(input) == 0) {
            free(input);
            continue;
        }
        
        // Добавляем команду в историю (кроме пустых)
        if (history && strlen(input) > 0) {
            add_to_history(history, input);
        }
        
        command_t *cmd = parse_input(input);
        if (cmd != NULL) {
            execute_command(cmd);
            free_command(cmd);
        }
        
        free(input);
    }
    
    // Сохраняем историю при выходе
    if (history) {
        save_history(history, HISTORY_FILE);
        printf("Command history saved (%d commands)\n", history->count);
        free_history(history);
    }
    
    printf("\nGoodbye!\n");
    return 0;
}