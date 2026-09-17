# ============================================================
# fsociety — Makefile implant (cross-compilation Windows)
# ============================================================

CC      = x86_64-w64-mingw32-gcc

# Répertoires
SRC_DIR    = src
INC_DIR    = include
COMMON_DIR = ../common
BUILD_DIR  = build

# Flags de compilation
CFLAGS  = -Wall -Wextra -O2 -std=c11 \
          -I$(INC_DIR) \
          -I$(COMMON_DIR)/include \
          -D_WIN32_WINNT=0x0600 \
          -DWIN32_LEAN_AND_MEAN

# Bibliothèques Windows nécessaires
LDFLAGS = -lws2_32 -lbcrypt -lcrypt32 -luser32

# Cible
TARGET  = fsociety-implant.exe

# Sources locales (implant)
SRCS_LOCAL  = $(wildcard $(SRC_DIR)/*.c)

# Sources partagées (common)
SRCS_COMMON = $(COMMON_DIR)/src/protocol.c

# Objets : build/<nom>.o pour toutes les sources
OBJS_LOCAL  = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS_LOCAL))
OBJS_COMMON = $(patsubst $(COMMON_DIR)/src/%.c,$(BUILD_DIR)/common_%.o,$(SRCS_COMMON))
OBJS        = $(OBJS_LOCAL) $(OBJS_COMMON)

# ---------- Règles ----------

all: $(TARGET)

$(TARGET): $(OBJS)
	@echo "[LD] $@"
	$(CC) -o $@ $^ $(LDFLAGS)
	@echo "[OK] $@ généré"

# Objets locaux : build/xxx.o depuis src/xxx.c
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "[CC] $<"
	$(CC) $(CFLAGS) -c $< -o $@

# Objets communs : build/common_xxx.o depuis ../common/src/xxx.c
$(BUILD_DIR)/common_%.o: $(COMMON_DIR)/src/%.c
	@mkdir -p $(dir $@)
	@echo "[CC] $<"
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	@rm -rf $(BUILD_DIR) $(TARGET)
	@echo "[OK] nettoyé"

fclean: clean

re: clean all

.PHONY: all clean fclean re