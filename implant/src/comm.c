#include "comm.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <string.h>

#pragma comment(lib, "ws2_32.lib")

/* ============================================================
 * fsociety — Implémentation communication (Windows Winsock)
 * ============================================================ */

/* ---------- Cycle de vie ---------- */

int fso_conn_init(void)
{
    WSADATA wsa;
    int ret = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (ret != 0) {
        fprintf(stderr, "[-] WSAStartup échoué : %d\n", ret);
        return -1;
    }
    return 0;
}

void fso_conn_cleanup(void)
{
    WSACleanup();
}

int fso_conn_connect(fso_conn_t *conn, const char *host, uint16_t port)
{
    if (!conn || !host) return -1;

    memset(conn, 0, sizeof(*conn));
    conn->sock = (uintptr_t)INVALID_SOCKET;
    strncpy(conn->host, host, sizeof(conn->host) - 1);
    conn->port = port;

    /* Création du socket TCP. */
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) {
        fprintf(stderr, "[-] socket échoué : %d\n", WSAGetLastError());
        return -1;
    }

    /* Résolution de l'adresse. */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (InetPtonA(AF_INET, host, &addr.sin_addr) != 1) {
        /* Pas une IP, essayer la résolution DNS. */
        struct addrinfo hints, *res = NULL;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        char port_str[8];
        snprintf(port_str, sizeof(port_str), "%u", port);

        if (getaddrinfo(host, port_str, &hints, &res) != 0 || !res) {
            fprintf(stderr, "[-] Résolution DNS échouée pour %s\n", host);
            closesocket(s);
            return -1;
        }
        memcpy(&addr, res->ai_addr, sizeof(addr));
        freeaddrinfo(res);
    }

    /* Connexion. */
    if (connect(s, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
        fprintf(stderr, "[-] connect échoué : %d\n", WSAGetLastError());
        closesocket(s);
        return -1;
    }

    conn->sock = (uintptr_t)s;
    conn->connected = 1;

    printf("[+] Connecté à %s:%u\n", host, port);
    return 0;
}

void fso_conn_close(fso_conn_t *conn)
{
    if (!conn) return;
    if (conn->sock != (uintptr_t)INVALID_SOCKET) {
        closesocket((SOCKET)conn->sock);
        conn->sock = (uintptr_t)INVALID_SOCKET;
    }
    conn->connected = 0;
}

/* ---------- I/O ---------- */

int fso_conn_send(fso_conn_t *conn, const void *buf, size_t len)
{
    if (!conn || !conn->connected) return -1;

    const char *p = buf;
    size_t total = 0;

    while (total < len) {
        int n = send((SOCKET)conn->sock, p + total, (int)(len - total), 0);
        if (n == SOCKET_ERROR) {
            fprintf(stderr, "[-] send échoué : %d\n", WSAGetLastError());
            return -1;
        }
        total += (size_t)n;
    }

    return (int)total;
}

int fso_conn_recv(fso_conn_t *conn, void *buf, size_t len)
{
    if (!conn || !conn->connected) return -1;

    int n = recv((SOCKET)conn->sock, buf, (int)len, 0);
    if (n == 0) {
        /* Déconnexion propre. */
        conn->connected = 0;
        return 0;
    }
    if (n == SOCKET_ERROR) {
        int err = WSAGetLastError();
        if (err != WSAETIMEDOUT) {
            fprintf(stderr, "[-] recv échoué : %d\n", err);
        }
        return -1;
    }

    return n;
}

/* ---------- Reconnexion ---------- */

int fso_conn_reconnect(fso_conn_t *conn)
{
    if (!conn) return -1;

    char host[256];
    uint16_t port;
    strncpy(host, conn->host, sizeof(host) - 1);
    host[sizeof(host) - 1] = '\0';
    port = conn->port;

    int delay = 5;  /* secondes */

    while (1) {
        printf("[*] Tentative de reconnexion dans %d s...\n", delay);
        Sleep(delay * 1000);

        fso_conn_close(conn);
        if (fso_conn_connect(conn, host, port) == 0) {
            printf("[+] Reconnexion réussie\n");
            return 0;
        }

        /* Backoff exponentiel, plafonné à 300 s. */
        delay *= 2;
        if (delay > 300) delay = 300;
    }

    return -1;
}