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
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_port = htons(4444);

    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) == SOCKET_ERR) {
        perror("connect");
        CLOSE_SOCKET(sock);
        CLEANUP_WINSOCK();
        return 1;
    }

    printf("[+] Connecté au serveur !\n");

    char *msg = "Hello from client!";
    char test[1024] = {0};
    while(1) {
        send(sock, msg, strlen(msg), 0);
        printf("[>] Envoyé: %s\n", msg);
        int n = recv(sock, test, sizeof(test) - 1, 0);
        if (n > 0) {
            test[n] = '\0';
            printf("[<] Reçu: %s\n", test);
        } 
        break;
    }

    CLOSE_SOCKET(sock);
    CLEANUP_WINSOCK();
    return 0;
}