#ifndef FSO_SERVER_H
#define FSO_SERVER_H

#include <stdint.h>
#include <netinet/in.h>

/* ============================================================
 * fsociety — Serveur C2
 * ============================================================ */

#define FSO_DEFAULT_PORT   8443
#define FSO_BACKLOG        10
#define FSO_MAX_CLIENTS    16

/* État d'un client connecté. */
typedef struct {
    int      fd;           /* descripteur de socket */
    uint32_t ip;           /* IP du client (network byte order) */
    uint16_t port;         /* port du client */
    int      active;       /* 1 si actif */
} fso_client_t;

/* État du serveur. */
typedef struct {
    int           listen_fd;                 /* socket d'écoute */
    uint16_t      port;                      /* port d'écoute */
    struct sockaddr_in addr;                 /* adresse d'écoute */
    fso_client_t  clients[FSO_MAX_CLIENTS];  /* clients connectés */
    int           nclients;                  /* nombre de clients actifs */
} fso_server_t;

/* ---------- Cycle de vie ---------- */

/* Initialise le serveur : crée le socket, bind, listen.
 * Retourne 0 en succès, -1 en erreur. */
int fso_server_init(fso_server_t *srv, uint16_t port);

/* Accepte une nouvelle connexion.
 * Retourne l'index du client dans le tableau, ou -1 en erreur. */
int fso_server_accept(fso_server_t *srv);

/* Ferme un client (fd + marque inactif).
 * Retourne 0 en succès, -1 en erreur. */
int fso_server_disconnect(fso_server_t *srv, int client_idx);

/* Ferme le serveur et tous les clients. */
void fso_server_close(fso_server_t *srv);

/* ---------- I/O ---------- */

/* Envoie `len` octets sur le client `idx`.
 * Retourne le nombre d'octets envoyés, ou -1 en erreur. */
ssize_t fso_server_send(fso_server_t *srv, int idx,
                        const void *buf, size_t len);

/* Reçoit jusqu'à `len` octets du client `idx`.
 * Retourne le nombre d'octets reçus, 0 si déconnexion, -1 en erreur. */
ssize_t fso_server_recv(fso_server_t *srv, int idx,
                        void *buf, size_t len);

/* ---------- Utilitaires ---------- */

/* Retourne une chaîne lisible pour l'IP du client `idx`.
 * Le buffer doit être au moins de taille INET_ADDRSTRLEN. */
const char *fso_server_client_ip(const fso_server_t *srv, int idx);

#endif /* FSO_SERVER_H */