#include "shell.h"

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

int main(void) {
    char *input;
    
    signal(SIGCHLD, check_child);
    
    printf("Shell R v7.2\n");
    printf("Type 'exit' to quit. Or use Ctrl + D\n");
    
    while ((input = read_line()) != NULL) {
        if (strlen(input) == 0) {
            free(input);
            continue;
        }
    
        // Парсим как единую команду с перенаправлениями
        command_t *cmd = parse_input(input);
        
        if (cmd != NULL) {
            // Для отладки - показываем что распарсилось print_command(cmd);
            
            // Выполняем команду
            execute_command(cmd);
            
            free_command(cmd);
        }        
    
        free(input);
    }
    
    printf("\nGoodbye!\n");
    return 0;
}