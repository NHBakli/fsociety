#include "crypto_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * fsociety — Test RSA côté C2 (Linux)
 * ============================================================ */

int main(void)
{
    printf("========================================\n");
    printf("  fsociety — Test RSA (Linux)\n");
    printf("========================================\n\n");

    fso_crypto_init();

    /* 1. Lecture de la clé publique DER. */
    printf("[*] Lecture de pub.der...\n");
    FILE *f = fopen("pub.der", "rb");
    if (!f) {
        printf("[-] pub.der introuvable (lance d'abord le test Windows)\n");
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long der_len = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *der = malloc((size_t)der_len);
    fread(der, 1, (size_t)der_len, f);
    fclose(f);
    printf("[+] pub.der lu (%ld octets)\n\n", der_len);

    /* 2. Chiffrement d'un message avec la clé publique. */
    const char *msg = "Hello from C2";
    printf("[*] Chiffrement de \"%s\"...\n", msg);

    uint8_t encrypted[512];
    size_t enc_len = sizeof(encrypted);
    if (fso_rsa_encrypt(der, (size_t)der_len,
                        (const uint8_t *)msg, strlen(msg),
                        encrypted, &enc_len) != FSO_CRYPTO_OK) {
        printf("[-] Échec chiffrement\n");
        free(der);
        return 1;
    }
    printf("[+] Chiffré (%zu octets)\n\n", enc_len);

    /* 3. Écriture du chiffré dans enc.bin. */
    f = fopen("enc.bin", "wb");
    if (!f) {
        printf("[-] Impossible d'écrire enc.bin\n");
        free(der);
        return 1;
    }
    fwrite(encrypted, 1, enc_len, f);
    fclose(f);
    printf("[+] enc.bin écrit\n\n");

    free(der);
    fso_crypto_cleanup();

    printf("========================================\n");
    printf("  [+] Fichier enc.bin prêt\n");
    printf("      Retourne côté Windows pour déchiffrer.\n");
    printf("========================================\n");

    return 0;
}