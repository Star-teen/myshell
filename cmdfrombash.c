#include "shell.h"

int from_bash_cd(char **args) {
    if (args[1] == NULL) {
        return 1;
    }

    if (args[2] != NULL) {
        perror("cd: too many arguments\n");
        return 1;
    }

    if (chdir(args[1]) != 0) {
        perror("cd");
        return 1;
    }
    return 0;
}

int from_bash_exit(char **args) {
    int exit_code = 0;

    if (args[1] != NULL) {
        exit_code = atoi(args[1]);
    }
    
    exit(exit_code);
}

int from_path(char **args) {
    char *path = getenv("PATH");
    if (path == NULL) {
        printf("PATH is not set\n");
    } else {
        printf("PATH=%s\n", path);
    }
    return 0;
}

int set_path(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "setpath: missing argument\n");
        fprintf(stderr, "Usage: setpath <new_path>\n");
        return 1;
    }

    if (args[2] != NULL) {
        fprintf(stderr, "setpath: too many arguments\n");
        fprintf(stderr, "Usage: setpath <new_path>\n");
        return 1;
    }

    if (setenv("PATH", args[1], 1) != 0) {
        perror("setpath");
        return 1;
    }
    
    printf("PATH set to: %s\n", args[1]);
    return 0;
}

int add_to_path(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "addpath: missing directory\n");
        fprintf(stderr, "Usage: addpath <directory>\n");
        return 1;
    }

    char *current_path = getenv("PATH");
    if (current_path == NULL) {
        // Если PATH не установлен, создаем новый
        if (setenv("PATH", args[1], 1) != 0) {
            perror("addpath");
            return 1;
        }
    } else {
        // Добавляем новую директорию в начало PATH
        size_t new_len = strlen(current_path) + strlen(args[1]) + 2;
        char *new_path = malloc(new_len);
        if (new_path == NULL) {
            perror("addpath: malloc");
            return 1;
        }
        
        snprintf(new_path, new_len, "%s:%s", args[1], current_path);
        
        if (setenv("PATH", new_path, 1) != 0) {
            perror("addpath");
            free(new_path);
            return 1;
        }
        
        free(new_path);
    }
    
    printf("Added '%s' to PATH\n", args[1]);
    printf("New PATH: %s\n", getenv("PATH"));
    return 0;
}

int reset_path(char **args) {
    const char *default_path = "/home/ruslan/.vscode-server/bin/ac4cbdf48759c7d8c3eb91ffe6bb04316e263c57/bin/remote-cli:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/usr/local/games:/mnt/c/Program Files (x86)/Common Files/Oracle/Java/javapath:/mnt/c/Windows/System32:/mnt/c/Windows:/mnt/c/Windows/System32/wbem:/mnt/c/Windows/System32/WindowsPowerShell/v1.0:/mnt/c/Windows/System32/OpenSSH:/mnt/c/Program Files (x86)/NVIDIA Corporation/PhysX/Common:/mnt/c/Code Write/FreePascal/bin/i386-Win32:/mnt/c/ProgramData/chocolatey/bin:/mnt/c/Program Files/Git/cmd:/mnt/c/Users/123/Desktop/MASM/bin:/mnt/c/msys64/ucrt64/bin:/mnt/c/Users/123/AppData/Local/Microsoft/WindowsApps:/mnt/c/Users/123/AppData/Local/Programs/Microsoft VS Code/bin:/mnt/c/Users/123/AppData/Local/Programs/Python/Python311:/snap/bin";
    
    if (setenv("PATH", default_path, 1) != 0) {
        perror("resetpath");
        return 1;
    }
    
    printf("PATH reset to default: %s\n", default_path);
    return 0;
}

int execute_bash_cmd(char **args) {
    if (args[0] == NULL) {
        return 1;
    }

    if (strcmp(args[0], "cd") == 0) {
        return from_bash_cd(args);
    } else if (strcmp(args[0], "exit") == 0) {
        return from_bash_exit(args);
    } else if (strcmp(args[0], "path") == 0) {
        return from_path(args);
    } else if (strcmp(args[0], "setpath") == 0) {
        return set_path(args);
    } else if (strcmp(args[0], "addpath") == 0) {
        return add_to_path(args);
    } else if (strcmp(args[0], "resetpath") == 0) {
        return reset_path(args);
    }

    return -1; // Не встроенная команда
}