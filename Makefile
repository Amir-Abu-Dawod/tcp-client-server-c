CC := gcc

CFLAGS := -std=c11 -Wall -Wextra -Werror

SRC_DIR := src
BUILD_DIR := build

ifeq ($(OS),Windows_NT)
    EXE := .exe
    LDLIBS := -lws2_32

    MKDIR_BUILD := if not exist "$(BUILD_DIR)" mkdir "$(BUILD_DIR)"
    CLEAN_BUILD := if exist "$(BUILD_DIR)" rmdir /S /Q "$(BUILD_DIR)"
else
    EXE :=
    CPPFLAGS := -D_POSIX_C_SOURCE=200809L
    LDLIBS :=

    MKDIR_BUILD := mkdir -p $(BUILD_DIR)
    CLEAN_BUILD := rm -rf $(BUILD_DIR)
endif

SERVER := $(BUILD_DIR)/server$(EXE)
CLIENT := $(BUILD_DIR)/client$(EXE)

.PHONY: all clean server client test

all: $(SERVER) $(CLIENT)

test: server
	python3 -m pytest -v

server: $(SERVER)

client: $(CLIENT)

$(BUILD_DIR):
	$(MKDIR_BUILD)

$(SERVER): $(SRC_DIR)/server.c $(SRC_DIR)/socket_platform.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SRC_DIR)/server.c -o $(SERVER) $(LDLIBS)

$(CLIENT): $(SRC_DIR)/client.c $(SRC_DIR)/socket_platform.h $(SRC_DIR)/timer_platform.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SRC_DIR)/client.c -o $(CLIENT) $(LDLIBS)

clean:
	$(CLEAN_BUILD)