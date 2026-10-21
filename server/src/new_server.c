#include "new_common.h"

static socket_t agent_sock = BAD_SOCKET;
static SSL *agent_tls = NULL;

static void disconnect_agent(void)
{
    tls_close(agent_tls, agent_sock);
    agent_tls = NULL;
    agent_sock = BAD_SOCKET;
    audit("Agent deconnecte.");
}

/* Chaque port exige le certificat du role attendu. */
static SSL *accept_peer(SSL_CTX *ctx, socket_t listener,
                        const char *name, socket_t *sock)
{
    *sock = accept(listener, NULL, NULL);
    if (*sock == BAD_SOCKET) {
        audit("Erreur accept.");
        return NULL;
    }
    SSL *tls = tls_session(ctx, *sock, 1, name);
    if (!tls) {
        audit("Connexion refusee : certificat attendu %s.", name);
        close_socket(*sock);
    }
    return tls;
}

static void serve_controller(SSL_CTX *ctx, socket_t listener)
{
    socket_t sock;
    SSL *tls = accept_peer(ctx, listener, "controller.lab", &sock);
    char command[LAB_SIZE], result[LAB_SIZE];
    if (!tls) return;
    audit("Controleur authentifie.");
    if (!write_line(tls, "READY") || read_line(tls, command, sizeof(command)) != 1) {
        audit("Controleur deconnecte ou requete invalide.");
        tls_close(tls, sock);
        return;
    }

    if (!allowed(command)) {
        strcpy(result, "REFUSED diagnostic inconnu");
        audit("Diagnostic refuse par le serveur.");
    } else if (!agent_tls) {
        strcpy(result, "ERROR agent deconnecte");
    } else {
        audit("Diagnostic demande : %s.", command);
        if (!write_line(agent_tls, command) ||
            read_line(agent_tls, result, sizeof(result)) != 1) {
            strcpy(result, "ERROR agent deconnecte ou delai depasse");
            disconnect_agent();
        }
    }
    if (!write_line(tls, result)) audit("Resultat non transmis au controleur.");
    tls_close(tls, sock);
}

int main(int argc, char **argv)
{
    const char *ip = argc == 2 ? argv[1] : "127.0.0.1";
    if (argc > 2 || !net_init()) return 1;
    SSL_CTX *ctx = tls_context("server", 1);
    if (!ctx) return 1;
    socket_t agents = net_listen(ip, AGENT_PORT);
    socket_t controllers = net_listen(ip, CONTROLLER_PORT);
    if (agents == BAD_SOCKET || controllers == BAD_SOCKET) {
        audit("Impossible d'ouvrir les ports du serveur.");
        if (agents != BAD_SOCKET) close_socket(agents);
        if (controllers != BAD_SOCKET) close_socket(controllers);
        SSL_CTX_free(ctx);
        return 1;
    }
    audit("Serveur sur %s : agent %d, controleur %d.", ip, AGENT_PORT, CONTROLLER_PORT);

    for (;;) {
        if (agent_tls && SSL_pending(agent_tls)) disconnect_agent();
        fd_set ready;
        FD_ZERO(&ready);
        FD_SET(agents, &ready);
        FD_SET(controllers, &ready);
        socket_t largest = agents > controllers ? agents : controllers;
        if (agent_tls) {
            FD_SET(agent_sock, &ready);
            if (agent_sock > largest) largest = agent_sock;
        }
        if (select((int)largest + 1, &ready, NULL, NULL, NULL) < 0) {
            audit("Erreur select : arret du serveur.");
            break;
        }
        /* L'agent n'envoie une reponse que lorsqu'un diagnostic est en cours. */
        if (agent_tls && FD_ISSET(agent_sock, &ready)) disconnect_agent();
        if (FD_ISSET(agents, &ready)) {
            socket_t sock;
            SSL *tls = accept_peer(ctx, agents, "agent.lab", &sock);
            if (tls) {
                if (agent_tls) {
                    if (!write_line(tls, "ERROR un agent est deja connecte"))
                        audit("Refus du second agent non transmis.");
                    audit("Second agent refuse.");
                    tls_close(tls, sock);
                } else {
                    agent_sock = sock;
                    agent_tls = tls;
                    if (!write_line(tls, "READY")) disconnect_agent();
                    else audit("Agent authentifie et disponible.");
                }
            }
        }
        if (FD_ISSET(controllers, &ready)) serve_controller(ctx, controllers);
    }
    if (agent_tls) disconnect_agent();
    close_socket(agents);
    close_socket(controllers);
    SSL_CTX_free(ctx);
    return 1;
}
