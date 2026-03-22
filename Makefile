# lpsh - A Unix Shell
# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Wall -Wextra -Wpedantic -std=c11 -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE

# Linker flags
LDFLAGS = -lreadline

# Directories
SRC_DIR = src
BIN_DIR = bin

# Source files
SRCS = $(wildcard $(SRC_DIR)/*.c)

# Object files
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(BIN_DIR)/%.o)

# Executable name
EXEC = $(BIN_DIR)/lpsh

# Default target
all: $(EXEC)

# Link object files to create the executable
$(EXEC): $(OBJS)
	$(CC) $(CFLAGS) -o $(EXEC) $(OBJS) $(LDFLAGS)

# Compile source files into object files
$(BIN_DIR)/%.o: $(SRC_DIR)/%.c | $(BIN_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Create bin directory if it doesn't exist
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Debug build with address sanitizer
debug: CFLAGS += -g -fsanitize=address -DDEBUG
debug: LDFLAGS += -fsanitize=address
debug: clean $(EXEC)

# Install to /usr/local/bin
install: $(EXEC)
	install -m 755 $(EXEC) /usr/local/bin/lpsh

# Uninstall
uninstall:
	rm -f /usr/local/bin/lpsh

# Clean up object files and executable
clean:
	rm -f $(OBJS) $(EXEC)

# Phony targets
.PHONY: all clean debug install uninstall