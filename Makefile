CC      := gcc
CFLAGS  := -Wall -Wextra -std=c11 -Iinclude -g
SRC_DIR := src
OBJ_DIR := obj
BIN_DIR := bin
TARGET  := $(BIN_DIR)/poker

SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

run: all
	./$(TARGET)

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

# --- Build WebAssembly (richiede Emscripten: https://emscripten.org/docs/getting_started/downloads.html) ---
# Attivare l'ambiente prima di lanciare 'make wasm': source /path/to/emsdk/emsdk_env.sh
WASM_SRCS := $(SRC_DIR)/deck.c $(SRC_DIR)/player.c $(SRC_DIR)/hand_eval.c $(SRC_DIR)/game.c $(SRC_DIR)/wasm_bridge.c
WEB_DIR   := web

.PHONY: wasm clean-wasm

wasm: $(WASM_SRCS)
	mkdir -p $(WEB_DIR)
	emcc $(WASM_SRCS) -Iinclude -O2 \
	  -s WASM=1 \
	  -s EXPORTED_FUNCTIONS="['_wasm_crea_partita','_wasm_nuova_mano','_wasm_avanza','_wasm_azione_umano','_wasm_stato_json']" \
	  -s EXPORTED_RUNTIME_METHODS="['ccall','cwrap']" \
	  -s ALLOW_MEMORY_GROWTH=1 \
	  -s MODULARIZE=1 -s EXPORT_NAME="PokerModule" \
	  -s ENVIRONMENT=web \
	  -o $(WEB_DIR)/poker.js

clean-wasm:
	rm -rf $(WEB_DIR)/poker.js $(WEB_DIR)/poker.wasm
