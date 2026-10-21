CC = gcc
CFLAGS ?= -std=c11 -D_DEFAULT_SOURCE -Wall -Wextra -Werror
LDLIBS = -lssl -lcrypto
EXE =
ifeq ($(OS),Windows_NT)
EXE = .exe
LDLIBS += -lws2_32 -ladvapi32
endif

.PHONY: all certs clean
all: client$(EXE) server$(EXE) new_ctrl$(EXE)

%$(EXE): %.c common.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -o $@ $(LDFLAGS) $(LDLIBS)

certs:
	sh make-certs.sh

clean:
	rm -f client client.exe server server.exe new_ctrl new_ctrl.exe
