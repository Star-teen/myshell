#include "shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Инициализация истории
history_t *init_history(int size) {
    history_t *hist = malloc(sizeof(history_t));
    if (!hist) {
        perror("malloc");
        return NULL;
    }
    
    hist->commands = malloc(size * sizeof(char*));
    if (!hist->commands) {
        perror("malloc");
        free(hist);
        return NULL;
    }
    
    // Инициализируем все указатели NULL
    for (int i = 0; i < size; i++) {
        hist->commands[i] = NULL;
    }
    
    hist->size = size;
    hist->count = 0;
    hist->current_index = -1;
    
    return hist;
}

// Освобождение памяти истории
void free_history(history_t *hist) {
    if (!hist) return;
    
    for (int i = 0; i < hist->count; i++) {
        free(hist->commands[i]);
    }
    free(hist->commands);
    free(hist);
}

// Добавление команды в историю
void add_to_history(history_t *hist, const char *command) {
    if (!hist || !command || strlen(command) == 0) {
        return;
    }
    
    // Пропускаем дубликаты (последняя команда)
    if (hist->count > 0 && strcmp(hist->commands[0], command) == 0) {
        return;
    }
    
    // Если история заполнена, удаляем самую старую команду
    if (hist->count == hist->size) {
        free(hist->commands[hist->size - 1]);
        
        // Сдвигаем команды вниз
        for (int i = hist->size - 1; i > 0; i--) {
            hist->commands[i] = hist->commands[i - 1];
        }
    } else {
        // Сдвигаем существующие команды
        for (int i = hist->count; i > 0; i--) {
            hist->commands[i] = hist->commands[i - 1];
        }
        hist->count++;
    }
    
    // Добавляем новую команду на вершину
    hist->commands[0] = strdup(command);
    if (!hist->commands[0]) {
        perror("strdup");
    }
    
    // Сбрасываем индекс навигации
    hist->current_index = -1;
}

// Сохранение истории в файл
void save_history(const history_t *hist, const char *filename) {
    if (!hist || !filename) return;
    
    FILE *f = fopen(filename, "w");
    if (!f) {
        perror("fopen");
        return;
    }
    
    // Сохраняем в обратном порядке (самые старые сначала)
    for (int i = hist->count - 1; i >= 0; i--) {
        if (hist->commands[i]) {
            fprintf(f, "%s\n", hist->commands[i]);
        }
    }
    
    fclose(f);
}

// Загрузка истории из файла
void load_history(history_t *hist, const char *filename) {
    if (!hist || !filename) return;
    
    FILE *f = fopen(filename, "r");
    if (!f) {
        // Файл может не существовать - это нормально
        return;
    }
    
    char line[MAX_HISTORY_LENGTH];
    
    while (fgets(line, sizeof(line), f)) {
        // Убираем перевод строки
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }
        
        // Пропускаем пустые строки
        if (strlen(line) > 0) {
            add_to_history(hist, line);
        }
    }
    
    fclose(f);
}

// Получение команды по индексу
char *get_history_command(const history_t *hist, int index) {
    if (!hist || index < 0 || index >= hist->count) {
        return NULL;
    }
    return hist->commands[index];
}

// Отладочная печать истории
void print_history(const history_t *hist) {
    if (!hist) {
        printf("History: NULL\n");
        return;
    }
    
    printf("Command History (%d/%d):\n", hist->count, hist->size);
    for (int i = 0; i < hist->count; i++) {
        printf("  %d: %s\n", i, hist->commands[i] ? hist->commands[i] : "NULL");
    }
}