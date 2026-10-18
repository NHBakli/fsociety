#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include "../include/client.h"

int main() {
    char cmd[2048] = {0};
    char buf[65536] = {0};

    printf("[+] Connecté au C2\n");

    while (1) {
        printf("controler> ");
        fflush(stdout);

        if (fgets(cmd, sizeof(cmd), stdin) == NULL) break;
        cmd[strcspn(cmd, "\n")] = 0;

        if (strcmp(cmd, "exit") == 0) break;
        if (strlen(cmd) == 0) continue;

        int sock = connect_to_server();
        printf("[*] Envoi de la commande & connexion au serveur...\n");
        if (sock == -1) continue;

        send(sock, cmd, strlen(cmd), 0);

        memset(buf, 0, sizeof(buf));
        int n = recv(sock, buf, sizeof(buf) - 1, 0);
        if (n > 0) {
            buf[n] = '\0';
            printf("%s\n", buf);
        }

        close(sock);
    }

    return 0;
}