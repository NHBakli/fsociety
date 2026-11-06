#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>

int main(void)
{
    char temp_directory[MAX_PATH];
    char log_path[MAX_PATH];

    DWORD directory_length = GetTempPathA(sizeof(temp_directory), temp_directory);
    if (directory_length == 0 || directory_length >= sizeof(temp_directory))
    {
        fputs("Cannot locate the temporary directory.\n", stderr);
        return 1;
    }

    int path_length = snprintf(
        log_path, sizeof(log_path), "%sfsociety-persistence-demo.log", temp_directory
    );
    if (path_length < 0 || (size_t)path_length >= sizeof(log_path))
    {
        fputs("The log path is too long.\n", stderr);
        return 1;
    }

    FILE *log = fopen(log_path, "a");
    if (log == NULL)
    {
        perror("Cannot open the demonstration log");
        return 1;
    }

    SYSTEMTIME started_at;
    GetLocalTime(&started_at);

    int written = fprintf(
        log, "%04u-%02u-%02u %02u:%02u:%02u pid=%lu\n",
        (unsigned)started_at.wYear,
        (unsigned)started_at.wMonth,
        (unsigned)started_at.wDay,
        (unsigned)started_at.wHour,
        (unsigned)started_at.wMinute,
        (unsigned)started_at.wSecond,
        (unsigned long)GetCurrentProcessId()
    );
    if (fclose(log) != 0 || written < 0)
    {
        fputs("Cannot write the demonstration log.\n", stderr);
        return 1;
    }

    printf("Demonstration launch recorded in: %s\n", log_path);
    return 0;
}
