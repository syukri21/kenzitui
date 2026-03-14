CC = gcc
CFLAGS = -Wall -Wextra -Werror -Iinclude -std=c11
LDFLAGS = -lncurses

SRC_DIR = src
INC_DIR = include
LIB_DIR = $(SRC_DIR)/lib
BIN_DIR = bin

OBJ_DIR = obj

# Main application files
LIB = $(wildcard $(LIB_DIR)/*.c)
SRC = src/main.c $(LIB)
TARGET = $(BIN_DIR)/kenzitui
OBJ = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRC))

# Test files
TEST_SRC = src/main_task_test.c
TEST_TARGET = $(BIN_DIR)/test_task
FETCH_TEST_SRC = src/main_fetch_test.c
FETCH_TEST_TARGET = $(BIN_DIR)/test_fetch

all: $(TARGET)

$(TARGET): $(OBJ) | $(BIN_DIR)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN_DIR):
	mkdir -p $@

$(OBJ_DIR):
	mkdir -p $@/lib

# Target to build and run tests
test: $(TEST_TARGET) $(FETCH_TEST_TARGET)
	./$(TEST_TARGET)
	./$(FETCH_TEST_TARGET)

$(TEST_TARGET): $(TEST_SRC) $(LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(TEST_SRC) $(LIB) -o $(TEST_TARGET) $(LDFLAGS)

$(FETCH_TEST_TARGET): $(FETCH_TEST_SRC) $(LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(FETCH_TEST_SRC) $(LIB) -o $(FETCH_TEST_TARGET) $(LDFLAGS)

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR) compile_commands.json

.PHONY: all clean test
