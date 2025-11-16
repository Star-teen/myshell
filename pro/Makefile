# Основные настройки
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -D_POSIX_C_SOURCE=200809L
SOURCES = main.c parcer.c executor.c cmdfrombash.c history.c terminal.c
OBJECTS = $(SOURCES:.c=.o)
TARGET = shell

# WSL лаунчер
LAUNCHER_SRC = launcher.c
LAUNCHER_TARGET = launch_shell

# Проверяем WSL
ifneq (,$(findstring WSL,$(shell uname -r)))
    WSL = 1
    CFLAGS += -DWSL_BUILD
endif

# Основная цель
$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(OBJECTS)

# WSL лаунчер
$(LAUNCHER_TARGET): $(LAUNCHER_SRC)
	$(CC) $(CFLAGS) -o $@ $(LAUNCHER_SRC)

# Компиляция объектных файлов
%.o: %.c shell.h
	$(CC) $(CFLAGS) -c $< -o $@

# Утилиты
all: $(TARGET) $(LAUNCHER_TARGET)

launcher: $(LAUNCHER_TARGET)

wsl-setup:
	@echo "WSL Environment Setup"
	@if [ -n "$(WSL)" ]; then \
		echo "WSL detected"; \
		sudo apt update && sudo apt install build-essential -y; \
	else \
		echo "Not running in WSL"; \
	fi

clean:
	rm -f $(OBJECTS) $(TARGET) $(LAUNCHER_TARGET) .myshell_history

# Фоновый запуск (только для WSL с X11)
run-background: $(TARGET)
	@if [ -n "$(DISPLAY)" ]; then \
		echo "Starting shell in background terminal..."; \
		./$(LAUNCHER_TARGET) 2; \
	else \
		echo "No X11 display. Starting in current terminal..."; \
		./$(TARGET); \
	fi

.PHONY: all launcher wsl-setup clean run-background