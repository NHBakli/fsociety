CC     = gcc
CFLAGS = -Wall -Wextra -O2 -Iinclude
SRCS = src/main.c src/utils.c src/session.c src/server.c src/controller.c src/logger.c
TARGET = fsociety-server
$(TARGET):
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET) -lpthread
clean:
	rm -f $(TARGET)
