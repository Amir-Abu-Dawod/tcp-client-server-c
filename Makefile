CC := gcc

CFLAGS := -std=c11 -Wall -Wextra -Werror

SRC_DIR := src
BUILD_DIR := build
REPORT_DIR := reports

ifeq ($(OS),Windows_NT) # Windows
    SHELL := cmd.exe
    
    EXE := .exe
    PYTHON := python
    LDLIBS := -lws2_32

    MKDIR_BUILD := if not exist "$(BUILD_DIR)" mkdir "$(BUILD_DIR)"
    MKDIR_REPORT := if not exist "$(REPORT_DIR)" mkdir "$(REPORT_DIR)"
    CLEAN_BUILD := if exist "$(BUILD_DIR)" rmdir /S /Q "$(BUILD_DIR)"

else # Linux
    EXE :=
    PYTHON := python3
    CPPFLAGS := -D_POSIX_C_SOURCE=200809L
    LDLIBS :=

    MKDIR_BUILD := mkdir -p $(BUILD_DIR)
    MKDIR_REPORT := mkdir -p $(REPORT_DIR)
    CLEAN_BUILD := rm -rf $(BUILD_DIR)
endif

SERVER := $(BUILD_DIR)/server$(EXE)
CLIENT := $(BUILD_DIR)/client$(EXE)

.PHONY: all clean server client test test-report

all: $(SERVER) $(CLIENT)

server: $(SERVER)

client: $(CLIENT)

test: server
	$(PYTHON) -m pytest -v

test-report: server | $(REPORT_DIR)
	$(PYTHON) -m pytest -v --color=no --junitxml=$(REPORT_DIR)/junit.xml

$(BUILD_DIR):
	$(MKDIR_BUILD)

$(REPORT_DIR):
	$(MKDIR_REPORT)

$(SERVER): $(SRC_DIR)/server.c $(SRC_DIR)/socket_platform.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SRC_DIR)/server.c -o $(SERVER) $(LDLIBS)

$(CLIENT): $(SRC_DIR)/client.c $(SRC_DIR)/socket_platform.h $(SRC_DIR)/timer_platform.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SRC_DIR)/client.c -o $(CLIENT) $(LDLIBS)

clean:
	$(CLEAN_BUILD)