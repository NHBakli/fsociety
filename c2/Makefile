# ============================================
# fsociety — C2 Makefile
# ============================================

CC      := gcc
CFLAGS  := -O2 -Wall -Wextra -Werror -std=c11 -D_GNU_SOURCE
LDFLAGS := -lssl -lcrypto

INCLUDE := -Iinclude -I../common/include
SRC := src/main.c src/server.c src/handshake.c ../common/src/crypto_common.c ../common/src/protocol.c
BIN     := fsociety-c2

.PHONY: all clean

all: $(BIN)

$(BIN): $(SRC)
	$(CC) $(CFLAGS) $(INCLUDE) $(SRC) -o $(BIN) $(LDFLAGS)

clean:
	rm -f $(BIN)