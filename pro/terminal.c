#include "shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>

static struct termios original_termios;

// Восстановление оригинальных настроек терминала
void restore_terminal(void) {
    tcsetattr(STDIN_FILENO, TCSANOW, &original_termios);
}

// Настройка неканонического режима терминала
int setup_terminal(void) {
    if (tcgetattr(STDIN_FILENO, &original_termios) == -1) {
        perror("tcgetattr");
        return -1;
    }
    
    struct termios new_termios = original_termios;
    new_termios.c_lflag &= ~(ICANON | ECHO);  // Отключаем канонический режим и эхо
    new_termios.c_cc[VMIN] = 1;   // Минимум 1 символ для чтения
    new_termios.c_cc[VTIME] = 0;  // Без таймаута
    
    if (tcsetattr(STDIN_FILENO, TCSANOW, &new_termios) == -1) {
        perror("tcsetattr");
        return -1;
    }
    
    // Регистрируем функцию восстановления при выходе
    atexit(restore_terminal);
    return 0;
}

// Очистка текущей строки в терминале
void clear_current_line(int current_pos) {
    for (int i = 0; i < current_pos; i++) {
        printf("\b \b");  // Backspace + space + backspace
    }
}