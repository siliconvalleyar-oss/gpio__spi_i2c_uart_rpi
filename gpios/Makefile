CC = g++
CXXFLAGS = -std=c++20 -Wall -pedantic -g
LIBRARIES = -pthread -lbcm2835
INCLUDES = -Isrc -Ilibs

SRC_DIR = src
LIB_DIR = libs
OBJ_DIR = obj
BIN_DIR = bin

APP = $(BIN_DIR)/gpio_generator

.PHONY: all clean run help

all: $(APP)

$(APP): | $(BIN_DIR)
$(APP): $(OBJ_DIR)/src/main.o \
       $(OBJ_DIR)/libs/port16/port16.o \
       $(OBJ_DIR)/libs/spi_master/spi_master.o \
       $(OBJ_DIR)/libs/i2c_master/i2c_master.o \
       $(OBJ_DIR)/libs/uart_master/uart_master.o \
       $(OBJ_DIR)/libs/onewire_master/onewire_master.o \
       $(OBJ_DIR)/libs/config/config.o
	$(CC) $(CXXFLAGS) $(INCLUDES) -o $@ $^ $(LIBRARIES)
	@echo "Binary created: $@"

$(OBJ_DIR)/%.o: %.cpp | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CXXFLAGS) $(INCLUDES) -c $< -o $@ -MMD -MP
	@echo "Compiled: $< -> $@"

$(BIN_DIR):
	@test -d $@ || mkdir -p $@

$(OBJ_DIR):
	@test -d $@ || mkdir -p $@

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
	@echo "Clean done"

run: $(APP)
	sudo ./$(APP)

help:
	@echo "Targets: all clean run help"
	@echo "Usage: make run [DURATION=120]"

-include $(wildcard $(OBJ_DIR)/**/*.d $(OBJ_DIR)/*.d)
