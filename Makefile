CC ?= cc
BASE_CFLAGS = -Wall -Wextra -Werror -Iinclude -std=c11
UNAME_S := $(shell uname -s)

NCURSES_LOCAL_PREFIX = third_party/ncurses/local
NCURSES_LOCAL_LIBS = \
	$(wildcard $(NCURSES_LOCAL_PREFIX)/lib/libncursesw.a) \
	$(wildcard $(NCURSES_LOCAL_PREFIX)/lib/libtinfow.a) \
	$(wildcard $(NCURSES_LOCAL_PREFIX)/lib/libtinfo.a)

PKG_NCURSES_CFLAGS = $(shell pkg-config --cflags ncursesw 2>/dev/null || pkg-config --cflags ncurses 2>/dev/null)
PKG_NCURSES_LIBS = $(shell pkg-config --libs ncursesw 2>/dev/null || pkg-config --libs ncurses 2>/dev/null)

BREW_NCURSES_PREFIX = $(shell brew --prefix ncurses 2>/dev/null)
BREW_NCURSES_CFLAGS = $(if $(BREW_NCURSES_PREFIX),-I$(BREW_NCURSES_PREFIX)/include)
BREW_NCURSES_LIBS = $(if $(BREW_NCURSES_PREFIX),-L$(BREW_NCURSES_PREFIX)/lib -lncursesw)

ifeq ($(UNAME_S),Darwin)
  SYS_NCURSES_CFLAGS = $(if $(strip $(PKG_NCURSES_CFLAGS)),$(PKG_NCURSES_CFLAGS),$(BREW_NCURSES_CFLAGS))
  SYS_NCURSES_LIBS = $(if $(strip $(PKG_NCURSES_LIBS)),$(PKG_NCURSES_LIBS),$(if $(strip $(BREW_NCURSES_LIBS)),$(BREW_NCURSES_LIBS),-lncurses))
else
  SYS_NCURSES_CFLAGS = $(PKG_NCURSES_CFLAGS)
  SYS_NCURSES_LIBS = $(if $(strip $(PKG_NCURSES_LIBS)),$(PKG_NCURSES_LIBS),-lncurses)
endif

ifeq ($(strip $(NCURSES_LOCAL_LIBS)),)
	CFLAGS = $(BASE_CFLAGS) $(SYS_NCURSES_CFLAGS)
	LDFLAGS = $(SYS_NCURSES_LIBS)
else
	CFLAGS = $(BASE_CFLAGS) -I$(NCURSES_LOCAL_PREFIX)/include -I$(NCURSES_LOCAL_PREFIX)/include/ncursesw
	LDFLAGS = $(NCURSES_LOCAL_LIBS)
endif

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

deps:
	./scripts/bootstrap_ncurses.sh

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR) compile_commands.json

.PHONY: all clean test deps
