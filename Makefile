CC = clang
AR = ar

CFLAGS = -Wall -Wextra -g -Iinc
LDFLAGS = -framework AudioToolbox -framework CoreFoundation -framework CoreAudio -lm

ifeq ($(SAN),asan) 
    CFLAGS += -fsanitize=address
    LDFLAGS += -fsanitize=address
else ifeq ($(SAN),tsan)
    CFLAGS += -fsanitize=thread
    LDFLAGS += -fsanitize=thread -pthread
endif

SRC_DIR = src
INC_DIR = inc
EXAMPLE_DIR = example
BUILD_DIR = build
LIB_DIR = $(BUILD_DIR)/lib

JAMLANG_SRCS = $(wildcard $(SRC_DIR)/*.c)
JAMLANG_OBJS = $(patsubst $(SRC_DIR)/%,$(BUILD_DIR)/%,$(JAMLANG_SRCS:.c=.o))

EXAMPLE_SRCS = $(EXAMPLE_DIR)/main.c
EXAMPLE_OBJS = $(patsubst $(EXAMPLE_DIR)/%.c,$(BUILD_DIR)/%.o,$(EXAMPLE_SRCS))

TARGET_LIB = $(LIB_DIR)/libjamlang.a
TARGET_EXE = $(BUILD_DIR)/jamlang_example

.PHONY: all clean dirs

all: dirs $(TARGET_EXE)

dirs:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(LIB_DIR)

$(TARGET_LIB): $(JAMLANG_OBJS)
	@echo "Creating static library: $@"
	$(AR) rcs $@ $(JAMLANG_OBJS)

$(TARGET_EXE): $(EXAMPLE_OBJS) $(TARGET_LIB)
	@echo "Linking executable: $@"
	$(CC) $(EXAMPLE_OBJS) $(TARGET_LIB) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@echo "Compiling library source: $<"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(EXAMPLE_DIR)/%.c
	@echo "Compiling example source: $<"
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	@echo "Cleaning build directory..."
	rm -rf $(BUILD_DIR)

fresh: clean all

run: all
	$(TARGET_EXE)

debug: $(TARGET_EXE)
	ASAN_OPTIONS="abort_on_error=1" lldb $(TARGET_EXE)

compile_commands: clean
	@echo "Generating compile_commands.json"
	bear -- make

