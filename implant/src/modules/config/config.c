#include <windows.h>
#include <stdio.h>
#include <shlobj.h>
#include "config.h"

void get_config_path(char *path, size_t max_len) {
    char appdata[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appdata))) {
        snprintf(path, max_len, "%s\\Microsoft\\config.dat", appdata);
    } else {
        snprintf(path, max_len, "C:\\Windows\\Temp\\config.dat");
    }
}

int config_load(char *ip_out, int *port_out) {
    char path[MAX_PATH];
    get_config_path(path, sizeof(path));

    FILE *f = fopen(path, "r");
    if (!f) return -1;

    if (fscanf(f, "%63s %d", ip_out, port_out) != 2) {
        fclose(f);
        return -1;
    }
    fclose(f);
    return 0;
}

int config_remove(void) {
    char path[MAX_PATH];
    get_config_path(path, sizeof(path));

    if (remove(path) == 0) {
        return 0; /* Succès */
    }
    return -1;
}

int config_save(const char *ip, int port) {
    char path[MAX_PATH];
    get_config_path(path, sizeof(path));

    FILE *f = fopen(path, "w");
    if (!f) return -1;

    fprintf(f, "%s %d\n", ip, port);
    fclose(f);
    return 0;
}