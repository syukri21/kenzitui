CC = gcc
CFLAGS = -Wall -Wextra -Werror -Iinclude -std=c11
LDFLAGS = -lncurses

SRC_DIR = src
INC_DIR = include
OBJ_DIR = obj
BIN_DIR = bin

# Main application files
SRC = src/main.c src/task.c src/kenzutls.c
OBJ = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRC))
TARGET = $(BIN_DIR)/kenzitui

# Test files
TEST_SRC = src/task.c src/main_task_test.c src/kenzutls.c
TEST_TARGET = $(BIN_DIR)/test_task

all: $(TARGET)

$(TARGET): $(OBJ) | $(BIN_DIR)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN_DIR) $(OBJ_DIR):
	mkdir -p $@

# Target to build and run the task test
test: $(TEST_SRC) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(TEST_SRC) -o $(TEST_TARGET)
	./$(TEST_TARGET)

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR) compile_commands.json

.PHONY: all clean test
