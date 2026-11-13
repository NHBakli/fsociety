#include <windows.h>
#include <stdio.h>
#include <time.h>

int main() {
    char path[MAX_PATH];
    GetTempPathA(MAX_PATH, path);
    strcat(path, "fsociety-persistence-demo.log");

    FILE *f = fopen(path, "a");
    if (f) {
        time_t t = time(NULL);
        struct tm *tm = localtime(&t);
        fprintf(f, "[%02d/%02d/%04d %02d:%02d:%02d] Processus ID: %lu\n",
                tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900,
                tm->tm_hour, tm->tm_min, tm->tm_sec,
                GetCurrentProcessId());
        fclose(f);
    }
    return 0;
}