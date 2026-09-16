#include <stdio.h>
#include <string.h>

#ifdef _WIN32
    #include <winsock2.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef int socklen_t;
    #define CLOSE_SOCKET closesocket
    #define SOCKET_TYPE SOCKET
    #define INVALID_SOCK INVALID_SOCKET
    #define SOCKET_ERR SOCKET_ERROR
    #define INIT_WINSOCK() do { WSADATA w; WSAStartup(MAKEWORD(2,2), &w); } while(0)
    #define CLEANUP_WINSOCK() WSACleanup()
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #define CLOSE_SOCKET close
    #define SOCKET_TYPE int
    #define INVALID_SOCK -1
    #define SOCKET_ERR -1
    #define INIT_WINSOCK() do {} while(0)
    #define CLEANUP_WINSOCK() do {} while(0)
#endif

int main() {
    INIT_WINSOCK();

    SOCKET_TYPE sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCK) {
        perror("socket");
        CLEANUP_WINSOCK();
        return 1;
    }

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(4444);

    if (bind(sock, (struct sockaddr*)&server, sizeof(server)) == SOCKET_ERR) {
        perror("bind");
        CLOSE_SOCKET(sock);
        CLEANUP_WINSOCK();
        return 1;
    }

    if (listen(sock, SOMAXCONN) == SOCKET_ERR) {
        perror("listen");
        CLOSE_SOCKET(sock);
        CLEANUP_WINSOCK();
        return 1;
    }

    printf("[*] En attente de connexion sur le port 4444...\n");

    struct sockaddr_in client_addr;
    socklen_t client_addr_size = sizeof(client_addr);
    SOCKET_TYPE client_sock = accept(sock, (struct sockaddr*)&client_addr, &client_addr_size);
    if (client_sock == INVALID_SOCK) {
        perror("accept");
        CLOSE_SOCKET(sock);
        CLEANUP_WINSOCK();
        return 1;
    }

    printf("[+] Client connecté !\n");

    char buffer[1024];
    while (1) {
        int bytes_received = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0) {
            printf("[!] Connexion fermée.\n");
            break;
        }
        buffer[bytes_received] = '\0';
        printf("[<] Reçu: %s\n", buffer);

        // Répondre "pong"
        char *response = "pong";
        send(client_sock, response, strlen(response), 0);
        printf("[>] Envoyé: %s\n", response);
    }

    CLOSE_SOCKET(client_sock);
    CLOSE_SOCKET(sock);
    CLEANUP_WINSOCK();
    return 0;
}