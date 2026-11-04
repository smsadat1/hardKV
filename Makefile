CC = gcc

TARGET_EXEC = db

BUILD_DIR = build
BUILD_TYPE ?= debug
SRC_DIR = src

SRCS = $(shell find $(SRC_DIR) -name '*.c')

# common flags
CFLAGS = -std=c17 \
    -Wall -Wextra -Wpedantic -Wshadow -Werror -Wconversion -Wformat=2 \
    -pthread

LDFLAGS = -pthread

# debug flags
DEBUG_CFLAGS = -O1 -g \
    -fanalyzer \
    -fsanitize=address,undefined \
    -fno-omit-frame-pointer \
    -DTS_DB_DBG

DEBUG_LDFLAGS = -fsanitize=address,undefined

# race test flags
RACE_TEST_CFLAGS = -O1 -g \
    -fsanitize=thread -fno-pie \
    -DTS_DB_DBG
RACE_TEST_LDFLAGS = -fsanitize=thread -no-pie

# release flags
RELEASE_CFLAGS = -O2 -DNDEBUG


.PHONY: all release debug race clean
all: release

release: $(BUILD_DIR)/release/$(TARGET_EXEC)
debug:  $(BUILD_DIR)/debug/$(TARGET_EXEC)
race:   $(BUILD_DIR)/race/$(TARGET_EXEC)

$(BUILD_DIR)/release/$(TARGET_EXEC): EXTRA_CFLAGS = $(RELEASE_CFLAGS)
$(BUILD_DIR)/release/$(TARGET_EXEC): EXTRA_LDFLAGS = $(RELEASE_LDFLAGS)

$(BUILD_DIR)/debug/$(TARGET_EXEC): EXTRA_CFLAGS = $(DEBUG_CFLAGS)
$(BUILD_DIR)/debug/$(TARGET_EXEC): EXTRA_LDFLAGS = $(DEBUG_LDFLAGS)

$(BUILD_DIR)/race/$(TARGET_EXEC): EXTRA_CFLAGS = $(RACE_TEST_CFLAGS)
$(BUILD_DIR)/race/$(TARGET_EXEC): EXTRA_LDFLAGS = $(RACE_TEST_LDFLAGS)

$(BUILD_DIR)/%/$(TARGET_EXEC): $(SRCS)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(EXTRA_CFLAGS) $(SRCS) \
	    -o $@ $(LDFLAGS) $(EXTRA_LDFLAGS)

clean:
	rm -rf $(BUILD_DIR)