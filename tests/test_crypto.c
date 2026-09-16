#include "crypto_common.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ============================================================
 * fsociety — Test unitaire de la crypto
 * ============================================================ */

static int test_aes(void)
{
    printf("[*] Test AES-256-GCM...\n");

    uint8_t key[FSO_AES_KEY_SIZE];
    if (fso_aes_generate_key(key) != FSO_CRYPTO_OK) {
        printf("[-] Échec génération clé AES\n");
        return -1;
    }

    const char *msg = "Hello, friend.";
    size_t msg_len = strlen(msg);

    uint8_t encrypted[512];
    size_t enc_len = 0;

    if (fso_aes_encrypt(key, (const uint8_t *)msg, msg_len,
                        encrypted, &enc_len) != FSO_CRYPTO_OK) {
        printf("[-] Échec chiffrement AES\n");
        return -1;
    }

    printf("    Chiffré : %zu octets (IV=%d + CT=%zu + TAG=%d)\n",
           enc_len, FSO_IV_SIZE, msg_len, FSO_TAG_SIZE);

    uint8_t decrypted[512];
    size_t dec_len = 0;

    if (fso_aes_decrypt(key, encrypted, enc_len,
                        decrypted, &dec_len) != FSO_CRYPTO_OK) {
        printf("[-] Échec déchiffrement AES\n");
        return -1;
    }
    decrypted[dec_len] = '\0';

    printf("    Déchiffré : \"%s\"\n", decrypted);

    if (strcmp((const char *)decrypted, msg) != 0) {
        printf("[-] Le message déchiffré ne correspond pas\n");
        return -1;
    }

    printf("[+] AES OK\n\n");
    return 0;
}

static int test_rsa(void)
{
    printf("[*] Test RSA-2048...\n");

    fso_rsa_keypair_t kp;
    if (fso_rsa_generate(&kp) != FSO_CRYPTO_OK) {
        printf("[-] Échec génération RSA\n");
        return -1;
    }

    printf("    Clé publique  : %zu octets\n", kp.pub_len);
    printf("    Clé privée    : %zu octets\n", kp.priv_len);

    /* Message à chiffrer avec RSA. */
    const char *msg = "fsociety";
    size_t msg_len = strlen(msg);

    uint8_t encrypted[512];
    size_t enc_len = 0;

    if (fso_rsa_encrypt(kp.pub, kp.pub_len,
                        (const uint8_t *)msg, msg_len,
                        encrypted, &enc_len) != FSO_CRYPTO_OK) {
        printf("[-] Échec chiffrement RSA\n");
        fso_rsa_free(&kp);
        return -1;
    }

    printf("    Chiffré RSA   : %zu octets\n", enc_len);

    uint8_t decrypted[512];
    size_t dec_len = 0;

    if (fso_rsa_decrypt(kp.priv, kp.priv_len,
                        encrypted, enc_len,
                        decrypted, &dec_len) != FSO_CRYPTO_OK) {
        printf("[-] Échec déchiffrement RSA\n");
        fso_rsa_free(&kp);
        return -1;
    }
    decrypted[dec_len] = '\0';

    printf("    Déchiffré RSA : \"%s\"\n", decrypted);

    if (strcmp((const char *)decrypted, msg) != 0) {
        printf("[-] Le message RSA déchiffré ne correspond pas\n");
        fso_rsa_free(&kp);
        return -1;
    }

    fso_rsa_free(&kp);
    printf("[+] RSA OK\n\n");
    return 0;
}

static int test_random(void)
{
    printf("[*] Test aléatoire...\n");

    uint8_t buf1[32];
    uint8_t buf2[32];

    if (fso_random_bytes(buf1, sizeof(buf1)) != FSO_CRYPTO_OK ||
        fso_random_bytes(buf2, sizeof(buf2)) != FSO_CRYPTO_OK) {
        printf("[-] Échec génération aléatoire\n");
        return -1;
    }

    if (memcmp(buf1, buf2, sizeof(buf1)) == 0) {
        printf("[-] Deux tirages identiques (improbable)\n");
        return -1;
    }

    printf("    Deux tirages différents : OK\n");
    printf("[+] Aléatoire OK\n\n");
    return 0;
}

int main(void)
{
    printf("========================================\n");
    printf("  fsociety — Test crypto\n");
    printf("========================================\n\n");

    if (fso_crypto_init() != FSO_CRYPTO_OK) {
        printf("[-] Échec init crypto\n");
        return 1;
    }

    int ret = 0;
    if (test_random() != 0) ret = 1;
    if (test_aes()    != 0) ret = 1;
    if (test_rsa()    != 0) ret = 1;

    fso_crypto_cleanup();

    printf("========================================\n");
    if (ret == 0) {
        printf("  [+] Tous les tests sont passés\n");
    } else {
        printf("  [-] Certains tests ont échoué\n");
    }
    printf("========================================\n");

    return ret;
}