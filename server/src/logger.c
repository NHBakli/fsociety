#include <stdio.h>
#include <time.h>

void log_info(const char *msg) {
    printf("[*] %s\n", msg);
}

void log_error(const char *msg) {
    printf("[!] %s\n", msg);
}