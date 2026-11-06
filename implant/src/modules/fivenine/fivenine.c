#include <windows.h>
#include "fivenine.h"

#define RUN_KEY "Software\\Microsoft\\Windows\\CurrentVersion\\Run"
#define VALUE_NAME "fsociety"
#define MUTEX_NAME "Global\\fsociety_fivenine"

int fivenine_install(void)
{
    char exe_path[MAX_PATH];
    if (GetModuleFileNameA(NULL, exe_path, MAX_PATH) == 0)
        return -1;

    HKEY hkey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, RUN_KEY, 0, KEY_SET_VALUE, &hkey) != ERROR_SUCCESS)
        return -1;

    LONG rc = RegSetValueExA(hkey, VALUE_NAME, 0, REG_SZ,
                             (const BYTE *)exe_path,
                             (DWORD)(strlen(exe_path) + 1));
    RegCloseKey(hkey);
    return (rc == ERROR_SUCCESS) ? 0 : -1;
}

int fivenine_uninstall(void)
{
    HKEY hkey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, RUN_KEY, 0, KEY_SET_VALUE, &hkey) != ERROR_SUCCESS)
        return -1;
    LONG rc = RegDeleteValueA(hkey, VALUE_NAME);
    RegCloseKey(hkey);
    return (rc == ERROR_SUCCESS) ? 0 : -1;
}