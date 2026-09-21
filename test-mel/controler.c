#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

int connect_to_server() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) { perror("socket"); return -1; }

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_port = htons(5555);

    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) == -1) {
        perror("connect"); close(sock); return -1;
    }
    return sock;
}

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