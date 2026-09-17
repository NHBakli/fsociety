#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <openssl/evp.h>
#include <openssl/rand.h>

#define IV_SIZE   12
#define TAG_SIZE  16
#define KEY_SIZE  32

static void hex_to_bytes(const char *hex, uint8_t *out, size_t out_len)
{
    for (size_t i = 0; i < out_len; i++) {
        unsigned int b;
        sscanf(hex + 2*i, "%02x", &b);
        out[i] = (uint8_t)b;
    }
}

int main(int argc, char **argv)
{
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <input> <output> <hex_key_64_chars>\n", argv[0]);
        fprintf(stderr, "Exemple de clé : 00112233445566778899aabbccddeeff"
                        "00112233445566778899aabbccddeeff\n");
        return 1;
    }

    const char *in_path  = argv[1];
    const char *out_path = argv[2];
    const char *hex_key  = argv[3];

    if (strlen(hex_key) != KEY_SIZE * 2) {
        fprintf(stderr, "[-] La clé doit faire %d caractères hex\n", KEY_SIZE * 2);
        return 1;
    }

    uint8_t key[KEY_SIZE];
    hex_to_bytes(hex_key, key, KEY_SIZE);

    /* 1. Lire le fichier d'entrée. */
    FILE *f = fopen(in_path, "rb");
    if (!f) { perror("fopen input"); return 1; }
    fseek(f, 0, SEEK_END);
    long in_len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (in_len <= 0) { fprintf(stderr, "[-] Fichier vide\n"); fclose(f); return 1; }

    uint8_t *in_buf = malloc((size_t)in_len);
    if (!in_buf) { fclose(f); return 1; }
    if (fread(in_buf, 1, (size_t)in_len, f) != (size_t)in_len) {
        fprintf(stderr, "[-] Lecture incomplète\n");
        fclose(f); free(in_buf); return 1;
    }
    fclose(f);

    printf("[*] Fichier lu : %ld octets\n", in_len);

    /* 2. Générer un IV aléatoire. */
    uint8_t iv[IV_SIZE];
    if (RAND_bytes(iv, IV_SIZE) != 1) {
        fprintf(stderr, "[-] RAND_bytes échoué\n");
        free(in_buf); return 1;
    }

    /* 3. Chiffrer avec AES-256-GCM. */
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) { free(in_buf); return 1; }

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1) {
        fprintf(stderr, "[-] EncryptInit échoué\n");
        goto fail;
    }
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, IV_SIZE, NULL) != 1) {
        fprintf(stderr, "[-] SetIVLen échoué\n");
        goto fail;
    }
    if (EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv) != 1) {
        fprintf(stderr, "[-] SetKey/IV échoué\n");
        goto fail;
    }

    uint8_t *ct = malloc((size_t)in_len + 16);
    if (!ct) goto fail;
    int ct_len = 0, tmp_len = 0;

    if (EVP_EncryptUpdate(ctx, ct, &ct_len, in_buf, (int)in_len) != 1) {
        fprintf(stderr, "[-] EncryptUpdate échoué\n");
        free(ct); goto fail;
    }
    if (EVP_EncryptFinal_ex(ctx, ct + ct_len, &tmp_len) != 1) {
        fprintf(stderr, "[-] EncryptFinal échoué\n");
        free(ct); goto fail;
    }
    ct_len += tmp_len;

    uint8_t tag[TAG_SIZE];
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, TAG_SIZE, tag) != 1) {
        fprintf(stderr, "[-] GetTag échoué\n");
        free(ct); goto fail;
    }

    /* 4. Écrire : IV || CT || TAG */
    FILE *out = fopen(out_path, "wb");
    if (!out) {
        perror("fopen output");
        free(ct); goto fail;
    }

    fwrite(iv,  1, IV_SIZE,  out);
    fwrite(ct,  1, (size_t)ct_len, out);
    fwrite(tag, 1, TAG_SIZE, out);
    fclose(out);

    printf("[+] Fichier chiffré : %s\n", out_path);
    printf("[+] IV  : ");
    for (int i = 0; i < IV_SIZE; i++)  printf("%02x", iv[i]);
    printf("\n");
    printf("[+] Tag : ");
    for (int i = 0; i < TAG_SIZE; i++) printf("%02x", tag[i]);
    printf("\n");
    printf("[+] Taille totale : %d octets\n", IV_SIZE + ct_len + TAG_SIZE);

    free(ct);
    free(in_buf);
    EVP_CIPHER_CTX_free(ctx);
    return 0;

fail:
    EVP_CIPHER_CTX_free(ctx);
    free(in_buf);
    return 1;
}