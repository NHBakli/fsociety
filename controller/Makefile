CC     = gcc
CFLAGS = -Wall -Wextra -O2 -Iinclude
SRCS   = src/main.c src/client.c
TARGET = fsociety-controller

all: $(TARGET)

$(TARGET):
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET)

clean:
	rm -f $(TARGET)
