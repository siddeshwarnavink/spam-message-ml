CC = cc
CFLAGS = -Ilib -lm -Wall -ggdb

BUILD_DIR = .build
SRC_DIR = src
LIB_DIR = lib

TARGET = $(BUILD_DIR)/main
SRC_FILES = $(wildcard $(SRC_DIR)/*.c)

all: $(TARGET)

$(TARGET): $(SRC_FILES) | $(BUILD_DIR)
	$(CC) -o $@ $^ $(CFLAGS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
