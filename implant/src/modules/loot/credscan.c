#include <string.h>
#include <ctype.h>
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif

static char result_buffer[65536];
static int  result_pos = 0;

void scan_file(const char *path)
{
    FILE *f = fopen(path, "r");
    if (f == NULL) return;

    char buf[1024];
    char lower[1024];
    const char *patterns[] = {"password", "api_key", "secret", "token", NULL};

    while (fgets(buf, sizeof(buf), f) != NULL) {
        for (int i = 0; buf[i]; i++)
            lower[i] = tolower(buf[i]);
        lower[strlen(buf)] = '\0';

        for (int i = 0; patterns[i] != NULL; i++) {
            if (strstr(lower, patterns[i])) {
                if (result_pos + (int)strlen(buf) < (int)sizeof(result_buffer) - 1) {
                    strcat(result_buffer, buf);
                    result_pos += strlen(buf);
                }
                break;
            }
        }
    }

    fclose(f);
}

void scan_directory(const char *path)
{
/* ═══ WINDOWS ═══ */
#ifdef _WIN32
    /* Windows n'a pas opendir — on utilise FindFirstFile/FindNextFile
     * il faut construire un pattern de recherche : path + "\*" */
    char search[2048];
    snprintf(search, sizeof(search), "%s\\*", path);

    WIN32_FIND_DATA fd;
    HANDLE h = FindFirstFile(search, &fd);
    if (h == INVALID_HANDLE_VALUE) return;

    do {
        /* ignorer "." (dossier courant) et ".." (dossier parent) */
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
            continue;

        /* construire le chemin complet : path\fichier */
        char full_path[2048];
        snprintf(full_path, sizeof(full_path), "%s\\%s", path, fd.cFileName);
        scan_file(full_path);
    } while (FindNextFile(h, &fd));

    FindClose(h);

/* ═══ MACOS / LINUX ═══ */
#else
    DIR *d = opendir(path);
    if (d == NULL) return;

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        /* ignorer "." et ".." */
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        /* construire le chemin complet : path/fichier */
        char full_path[2048];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        scan_file(full_path);
    }

    closedir(d);
#endif
}

char *scan_credentials(void)
{
    memset(result_buffer, 0, sizeof(result_buffer));
    result_pos = 0;

    scan_directory(".");
    return result_buffer;
}
