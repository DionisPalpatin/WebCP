# Компилятор и флаги
CC = gcc
CFLAGS = -Wall -Wextra -Werror -Iinc -g
LDFLAGS =

# Директории
SRC_DIR = server
INC_DIR = inc

# Файлы
MAIN = main.c
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(SRCS:.c=.o)

# Целевое имя
TARGET = app.exe

# Сборка
all: $(TARGET)

$(TARGET): $(OBJS) $(MAIN)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c $(INC_DIR)/*.h
	$(CC) $(CFLAGS) -c $< -o $@

# Очистка
clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(TARGET)

re: fclean all

# Утилиты
.PHONY: all clean fclean re
