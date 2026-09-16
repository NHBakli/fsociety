#include "crypto.h"
#include "rsa_der.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * fsociety — Test RSA côté implant (Windows)
 * ============================================================ */

int main(int argc, char **argv)
{
    printf("========================================\n");
    printf("  fsociety — Test RSA (Windows)\n");
    printf("========================================\n\n");

    fso_crypto_init();

    /* 1. Génération de la paire RSA. */
    printf("[*] Génération RSA-2048...\n");
    fso_rsa_keypair_t kp;
    if (fso_rsa_generate(&kp) != FSO_CRYPTO_OK) {
        printf("[-] Échec génération\n");
        return 1;
    }
    printf("[+] Paire générée (pub=%zu, priv=%zu)\n\n", kp.pub_len, kp.priv_len);

    /* 2. Conversion de la clé publique en DER. */
    printf("[*] Conversion BCrypt → DER...\n");
    uint8_t der[1024];
    size_t der_len = sizeof(der);
    if (fso_rsa_bcrypt_to_der(kp.pub, kp.pub_len, der, &der_len) != FSO_CRYPTO_OK) {
        printf("[-] Échec conversion\n");
        fso_rsa_free(&kp);
        return 1;
    }
    printf("[+] DER généré (%zu octets)\n\n", der_len);

    /* 3. Écriture de la clé publique DER dans un fichier. */
    FILE *f = fopen("pub.der", "wb");
    if (!f) {
        printf("[-] Impossible d'écrire pub.der\n");
        fso_rsa_free(&kp);
        return 1;
    }
    fwrite(der, 1, der_len, f);
    fclose(f);
    printf("[+] Clé publique écrite dans pub.der\n\n");

    /* 4. Attente du fichier `enc.bin` (chiffré par Linux). */
    printf("[*] En attente de enc.bin (chiffré par le C2)...\n");
    printf("    Lance le test Linux, puis appuie sur Entrée.\n");
    getchar();

    f = fopen("enc.bin", "rb");
    if (!f) {
        printf("[-] enc.bin introuvable\n");
        fso_rsa_free(&kp);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long enc_len = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *enc = malloc((size_t)enc_len);
    fread(enc, 1, (size_t)enc_len, f);
    fclose(f);
    printf("[+] enc.bin lu (%ld octets)\n\n", enc_len);

    /* 5. Déchiffrement avec la clé privée. */
    printf("[*] Déchiffrement RSA...\n");
    uint8_t decrypted[512];
    size_t dec_len = sizeof(decrypted);
    if (fso_rsa_decrypt(kp.priv, kp.priv_len, enc, (size_t)enc_len,
                        decrypted, &dec_len) != FSO_CRYPTO_OK) {
        printf("[-] Échec déchiffrement\n");
        free(enc);
        fso_rsa_free(&kp);
        return 1;
    }
    decrypted[dec_len] = '\0';
    printf("[+] Déchiffré : \"%s\"\n\n", decrypted);

    free(enc);
    fso_rsa_free(&kp);
    fso_crypto_cleanup();

    printf("========================================\n");
    printf("  [+] Test RSA terminé\n");
    printf("========================================\n");

    return 0;
}