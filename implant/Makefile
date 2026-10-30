CC     = gcc
CFLAGS = -Wall -Wextra -O2 -Iinclude
SRCS   = src/main.c
TARGET = fsociety-implant

all: $(TARGET)

$(TARGET):
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET)

clean:
	rm -f $(TARGET)
