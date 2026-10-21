#include "common.h"

/* Une connexion authentifiee par diagnostic, sans commande libre. */
static int request(SSL_CTX *ctx, const char *ip, const char *command)
{
    char result[LAB_SIZE];
    socket_t sock = net_connect(ip, CONTROLLER_PORT);
    if (sock == BAD_SOCKET) {
        audit("Connexion au serveur impossible.");
        return 0;
    }
    SSL *tls = tls_session(ctx, sock, 0, "server.lab");
    if (!tls) {
        audit("Authentification refusee.");
        close_socket(sock);
        return 0;
    }
    int ok = read_line(tls, result, sizeof(result)) == 1 &&
             strcmp(result, "READY") == 0 && write_line(tls, command) &&
             net_timeout(sock, 10) && /* Laisser au relais ses 5 s pour repondre. */
             read_line(tls, result, sizeof(result)) == 1;
    if (ok) puts(result);
    else audit("Serveur deconnecte, reponse invalide ou delai depasse.");
    tls_close(tls, sock);
    return ok && strncmp(result, "OK ", 3) == 0;
}

int main(int argc, char **argv)
{
    const char *commands[] = {"hostname", "os", "time"};
    const char *ip = argc >= 2 ? argv[1] : "127.0.0.1";
    if (argc > 3 || (argc == 3 && !allowed(argv[2]))) {
        fprintf(stderr, "Usage : %s [IP [hostname|os|time]]\n", argv[0]);
        return 1;
    }
    if (!net_init()) return 1;
    SSL_CTX *ctx = tls_context("controller", 0);
    if (!ctx) return 1;
    int status = 0;
    if (argc == 3) {
        status = request(ctx, ip, argv[2]) ? 0 : 1;
    } else {
        char choice[LAB_SIZE];
        for (;;) {
            printf("\n1. Nom de la machine\n2. Systeme\n3. Heure UTC\n0. Quitter\nChoix : ");
            fflush(stdout);
            if (!fgets(choice, sizeof(choice), stdin)) break;
            choice[strcspn(choice, "\r\n")] = '\0';
            if (strcmp(choice, "0") == 0) break;
            if (strlen(choice) != 1 || choice[0] < '1' || choice[0] > '3') {
                puts("Choix invalide.");
                continue;
            }
            status = request(ctx, ip, commands[choice[0] - '1']) ? 0 : 1;
        }
    }
    SSL_CTX_free(ctx);
    return status;
}
