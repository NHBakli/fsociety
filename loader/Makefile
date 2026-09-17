# ============================================================
# fsociety — Makefile loader (cross-compilation Windows)
# ============================================================

CC      = x86_64-w64-mingw32-gcc

SRC_DIR   = src
INC_DIR   = include
BUILD_DIR = build

CFLAGS  = -Wall -Wextra -O2 -std=c11 \
          -I$(INC_DIR) \
          -D_WIN32_WINNT=0x0600 \
          -DWIN32_LEAN_AND_MEAN

LDFLAGS = -lbcrypt

TARGET  = loader.exe

SRCS    = $(wildcard $(SRC_DIR)/*.c)
OBJS    = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))

all: $(TARGET)

$(TARGET): $(OBJS)
	@echo "[LD] $@"
	$(CC) -o $@ $^ $(LDFLAGS)
	@echo "[OK] $@ généré"

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "[CC] $<"
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	@rm -rf $(BUILD_DIR) $(TARGET)
	@echo "[OK] nettoyé"

re: clean all

.PHONY: all clean re