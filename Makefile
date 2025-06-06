# --- Configuration ---
CC = clang
AR = ar          # Archiver for creating static libraries
CFLAGS = -Wall -Wextra -g -Iinc # -Iinc tells the compiler to look in the 'inc' directory for headers

# Common linker flags for Core Audio frameworks on macOS
LDFLAGS_COREAUDIO = -framework AudioToolbox -framework CoreFoundation -framework CoreAudio -lm

# --- Directories ---
SRC_DIR = src
INC_DIR = inc
EXAMPLE_DIR = example
BUILD_DIR = build
LIB_DIR = $(BUILD_DIR)/lib

# --- Source and Object Files ---
# JAMLANG_SRCS: Automatically find all .c files in SRC_DIR
# Using 'wildcard' to glob the files
JAMLANG_SRCS = $(wildcard $(SRC_DIR)/*.c)

# JAMLANG_OBJS: Convert source file paths to object file paths in the build directory
# $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(JAMLANG_SRCS))
# For each $(SRC_DIR)/file.c, produce $(BUILD_DIR)/file.o
JAMLANG_OBJS = $(patsubst $(SRC_DIR)/%,$(BUILD_DIR)/%,$(JAMLANG_SRCS:.c=.o))

# Example sources (still assuming a single main.c for simplicity)
EXAMPLE_SRCS = $(EXAMPLE_DIR)/main.c
EXAMPLE_OBJS = $(patsubst $(EXAMPLE_DIR)/%.c,$(BUILD_DIR)/%.o,$(EXAMPLE_SRCS))

# --- Targets ---
TARGET_LIB = $(LIB_DIR)/libjamlang.a
TARGET_EXE = $(BUILD_DIR)/jamlang_example

.PHONY: all clean dirs

# Default target: build the example executable
all: dirs $(TARGET_EXE)

# Create build directories
dirs:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(LIB_DIR)

# Rule to build the static library
$(TARGET_LIB): $(JAMLANG_OBJS)
	@echo "Creating static library: $@"
	$(AR) rcs $@ $(JAMLANG_OBJS)

# Rule to build the example executable
# It depends on its own object files and the static library
$(TARGET_EXE): $(EXAMPLE_OBJS) $(TARGET_LIB)
	@echo "Linking executable: $@"
	$(CC) $(EXAMPLE_OBJS) $(TARGET_LIB) -o $@ $(LDFLAGS_COREAUDIO)

# Generic rule to compile .c files to .o files in the build directory
# This handles both library and example source files
# The order of these two rules matters if file names conflict (e.g. main.c in both)
# But with distinct directories, it works fine.
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@echo "Compiling library source: $<"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(EXAMPLE_DIR)/%.c
	@echo "Compiling example source: $<"
	$(CC) $(CFLAGS) -c $< -o $@


# Clean target: remove all generated files
clean:
	@echo "Cleaning build directory..."
	rm -rf $(BUILD_DIR)

# Clean then rebuild
fresh: clean all

# Run the example
run: all
	$(TARGET_EXE)

# Debug the example
debug: fresh
	lldb $(TARGET_EXE)

# Generate compile_commands.json for LSP
compile_commands: clean
	@echo "Generating compile_commands.json"
	bear -- make

