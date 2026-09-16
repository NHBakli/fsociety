#ifndef FSO_HANDSHAKE_H
#define FSO_HANDSHAKE_H

#include "server.h"
#include <stdint.h>

/* Gère l'échange de clé initial avec un client.
 * Attend MSG_KEYX, envoie MSG_AUTH.
 * Stocke la clé AES dans `aes_key_out`.
 * Retourne 0 en succès, -1 en erreur. */
int fso_handshake(fso_server_t *srv, int client_idx,
                  uint8_t *aes_key_out);

#endif /* FSO_HANDSHAKE_H */