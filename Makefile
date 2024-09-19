# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Wall -Wextra -std=c11

# Directories
SRC_DIR = src
BIN_DIR = bin

# Source files
SRCS = $(wildcard $(SRC_DIR)/*.c)

# Object files
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(BIN_DIR)/%.o)

# Executable name
EXEC = $(BIN_DIR)/myshell

# Default target
all: $(EXEC)

# Link object files to create the executable
$(EXEC): $(OBJS)
	$(CC)	$(CFLAGS)	-o	$(EXEC)	$(OBJS)

# Compile source files into object files
$(BIN_DIR)/%.o: $(SRC_DIR)/%.c | $(BIN_DIR)
	$(CC)	$(CFLAGS)	-c	$<	-o	$@

# Create bin directory if it doesn't exist
$(BIN_DIR):
	mkdir	-p	$(BIN_DIR)

# Clean up object files and executable
clean:
	rm	-f	$(OBJS)	$(EXEC)

# Phony targets
.PHONY: all clean