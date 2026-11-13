#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "injection.h"
#include <tlhelp32.h>

int inject_into_process(const char *target_process, const char *dll_path) {
    // 1. Trouver le PID du processus cible (explorer.exe)
    DWORD pid = 0;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32);
        if (Process32First(hSnapshot, &pe32)) {
            do {
                if (_stricmp(pe32.szExeFile, target_process) == 0) {
                    pid = pe32.th32ProcessID;
                    break;
                }
            } while (Process32Next(hSnapshot, &pe32));
        }
        CloseHandle(hSnapshot);
    }

    if (pid == 0) return -1; // Processus cible non trouvé

    // 2. Ouvrir le processus
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess) return -1;

    // 3. Allouer la mémoire pour le chemin de la DLL
    size_t dll_len = strlen(dll_path) + 1;
    LPVOID pRemoteMem = VirtualAllocEx(hProcess, NULL, dll_len, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pRemoteMem) {
        CloseHandle(hProcess);
        return -1;
    }

    // 4. Écrire le chemin de la DLL dans la mémoire distante
    if (!WriteProcessMemory(hProcess, pRemoteMem, dll_path, dll_len, NULL)) {
        VirtualFreeEx(hProcess, pRemoteMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return -1;
    }

    // 5. Trouver l'adresse de LoadLibraryA (qui est la même dans tous les processus)
    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    LPVOID pLoadLibrary = (LPVOID)GetProcAddress(hKernel32, "LoadLibraryA");

    // 6. Créer le thread distant qui exécute LoadLibraryA(chemin_de_la_dll)
    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0,
                                       (LPTHREAD_START_ROUTINE)pLoadLibrary,
                                       pRemoteMem, 0, NULL);
    if (!hThread) {
        VirtualFreeEx(hProcess, pRemoteMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return -1;
    }

    // 7. Attendre que la DLL soit chargée (max 5 secondes)
    WaitForSingleObject(hThread, 5000);

    // Nettoyage (on ne libère pas pRemoteMem pour éviter un crash de la DLL)
    CloseHandle(hThread);
    CloseHandle(hProcess);

    return 0;
}