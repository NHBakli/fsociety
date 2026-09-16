#include "crypto_common.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/err.h>

/* ============================================================
 * fsociety — Implémentation crypto (OpenSSL, côté C2)
 * ============================================================ */

/* ---------- Init / cleanup ---------- */

int fso_crypto_init(void)
{
    /* OpenSSL 1.1+ s'initialise tout seul. */
    return FSO_CRYPTO_OK;
}

void fso_crypto_cleanup(void)
{
    /* Rien à faire avec OpenSSL 1.1+. */
}

/* ---------- Aléatoire ---------- */

int fso_random_bytes(uint8_t *buf, size_t len)
{
    if (!buf || len == 0) {
        return FSO_CRYPTO_ERR_INVALID;
    }
    if (RAND_bytes(buf, (int)len) != 1) {
        return FSO_CRYPTO_ERR_RANDOM;
    }
    return FSO_CRYPTO_OK;
}

/* ---------- RSA ---------- */

int fso_rsa_generate(fso_rsa_keypair_t *keypair)
{
    if (!keypair) {
        return FSO_CRYPTO_ERR_INVALID;
    }

    memset(keypair, 0, sizeof(*keypair));

    /* Génération de la paire RSA-2048. */
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
    if (!ctx) {
        return FSO_CRYPTO_ERR_KEYGEN;
    }

    if (EVP_PKEY_keygen_init(ctx) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return FSO_CRYPTO_ERR_KEYGEN;
    }

    if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, FSO_RSA_KEY_BITS) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return FSO_CRYPTO_ERR_KEYGEN;
    }

    EVP_PKEY *pkey = NULL;
    if (EVP_PKEY_keygen(ctx, &pkey) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return FSO_CRYPTO_ERR_KEYGEN;
    }
    EVP_PKEY_CTX_free(ctx);

    /* Sérialisation de la clé publique en DER. */
    unsigned char *pub_der = NULL;
    int pub_len = i2d_PUBKEY(pkey, &pub_der);
    if (pub_len <= 0) {
        EVP_PKEY_free(pkey);
        return FSO_CRYPTO_ERR_KEYGEN;
    }

    /* Sérialisation de la clé privée en DER. */
    unsigned char *priv_der = NULL;
    int priv_len = i2d_PrivateKey(pkey, &priv_der);
    if (priv_len <= 0) {
        OPENSSL_free(pub_der);
        EVP_PKEY_free(pkey);
        return FSO_CRYPTO_ERR_KEYGEN;
    }

    /* Copie dans la structure. */
    keypair->pub = malloc((size_t)pub_len);
    keypair->priv = malloc((size_t)priv_len);
    if (!keypair->pub || !keypair->priv) {
        free(keypair->pub);
        free(keypair->priv);
        OPENSSL_free(pub_der);
        OPENSSL_free(priv_der);
        EVP_PKEY_free(pkey);
        return FSO_CRYPTO_ERR_MEMORY;
    }

    memcpy(keypair->pub, pub_der, (size_t)pub_len);
    memcpy(keypair->priv, priv_der, (size_t)priv_len);
    keypair->pub_len = (size_t)pub_len;
    keypair->priv_len = (size_t)priv_len;

    OPENSSL_free(pub_der);
    OPENSSL_free(priv_der);
    EVP_PKEY_free(pkey);

    return FSO_CRYPTO_OK;
}

int fso_rsa_encrypt(const uint8_t *pub, size_t pub_len,
                    const uint8_t *in, size_t in_len,
                    uint8_t *out, size_t *out_len)
{
    if (!pub || !in || !out || !out_len) {
        return FSO_CRYPTO_ERR_INVALID;
    }

    /* Reconstruction de la clé publique depuis DER. */
    const unsigned char *p = pub;
    EVP_PKEY *pkey = d2i_PUBKEY(NULL, &p, (long)pub_len);
    if (!pkey) {
        return FSO_CRYPTO_ERR_FORMAT;
    }

    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(pkey, NULL);
    if (!ctx) {
        EVP_PKEY_free(pkey);
        return FSO_CRYPTO_ERR_ENCRYPT;
    }

    if (EVP_PKEY_encrypt_init(ctx) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        return FSO_CRYPTO_ERR_ENCRYPT;
    }

    if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        return FSO_CRYPTO_ERR_ENCRYPT;
    }

    if (EVP_PKEY_CTX_set_rsa_oaep_md(ctx, EVP_sha256()) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        return FSO_CRYPTO_ERR_ENCRYPT;  /* ou DECRYPT selon la fonction */
    }

    if (EVP_PKEY_CTX_set_rsa_mgf1_md(ctx, EVP_sha256()) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        return FSO_CRYPTO_ERR_ENCRYPT;
    }

    size_t len = 0;
    if (EVP_PKEY_encrypt(ctx, NULL, &len, in, in_len) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        return FSO_CRYPTO_ERR_ENCRYPT;
    }

    if (EVP_PKEY_encrypt(ctx, out, &len, in, in_len) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        return FSO_CRYPTO_ERR_ENCRYPT;
    }

    *out_len = len;

    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(pkey);

    return FSO_CRYPTO_OK;
}

int fso_rsa_decrypt(const uint8_t *priv, size_t priv_len,
                    const uint8_t *in, size_t in_len,
                    uint8_t *out, size_t *out_len)
{
    if (!priv || !in || !out || !out_len) {
        return FSO_CRYPTO_ERR_INVALID;
    }

    const unsigned char *p = priv;
    EVP_PKEY *pkey = d2i_AutoPrivateKey(NULL, &p, (long)priv_len);
    if (!pkey) {
        return FSO_CRYPTO_ERR_FORMAT;
    }

    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(pkey, NULL);
    if (!ctx) {
        EVP_PKEY_free(pkey);
        return FSO_CRYPTO_ERR_DECRYPT;
    }

    if (EVP_PKEY_decrypt_init(ctx) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        return FSO_CRYPTO_ERR_DECRYPT;
    }

    if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        return FSO_CRYPTO_ERR_DECRYPT;
    }

    size_t len = 0;
    if (EVP_PKEY_decrypt(ctx, NULL, &len, in, in_len) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        return FSO_CRYPTO_ERR_DECRYPT;
    }

    if (EVP_PKEY_decrypt(ctx, out, &len, in, in_len) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        return FSO_CRYPTO_ERR_DECRYPT;
    }

    *out_len = len;

    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(pkey);

    return FSO_CRYPTO_OK;
}

void fso_rsa_free(fso_rsa_keypair_t *keypair)
{
    if (!keypair) {
        return;
    }
    free(keypair->pub);
    free(keypair->priv);
    memset(keypair, 0, sizeof(*keypair));
}

/* ---------- AES-256-GCM ---------- */

int fso_aes_generate_key(uint8_t *key)
{
    return fso_random_bytes(key, FSO_AES_KEY_SIZE);
}

int fso_aes_encrypt(const uint8_t *key,
                    const uint8_t *plaintext, size_t pt_len,
                    uint8_t *out, size_t *out_len)
{
    if (!key || !plaintext || !out || !out_len) {
        return FSO_CRYPTO_ERR_INVALID;
    }

    /* IV aléatoire de 12 octets. */
    uint8_t iv[FSO_IV_SIZE];
    if (fso_random_bytes(iv, FSO_IV_SIZE) != FSO_CRYPTO_OK) {
        return FSO_CRYPTO_ERR_RANDOM;
    }

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return FSO_CRYPTO_ERR_ENCRYPT;
    }

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return FSO_CRYPTO_ERR_ENCRYPT;
    }

    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, FSO_IV_SIZE, NULL) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return FSO_CRYPTO_ERR_ENCRYPT;
    }

    if (EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return FSO_CRYPTO_ERR_ENCRYPT;
    }

    /* Copie de l'IV dans la sortie. */
    memcpy(out, iv, FSO_IV_SIZE);

    int len = 0;
    int total = 0;
    if (EVP_EncryptUpdate(ctx, out + FSO_IV_SIZE, &len,
                          plaintext, (int)pt_len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return FSO_CRYPTO_ERR_ENCRYPT;
    }
    total = len;

    if (EVP_EncryptFinal_ex(ctx, out + FSO_IV_SIZE + total, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return FSO_CRYPTO_ERR_ENCRYPT;
    }
    total += len;

    /* Récupération du tag. */
    uint8_t tag[FSO_TAG_SIZE];
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, FSO_TAG_SIZE, tag) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return FSO_CRYPTO_ERR_ENCRYPT;
    }

    /* Copie du tag après le ciphertext. */
    memcpy(out + FSO_IV_SIZE + total, tag, FSO_TAG_SIZE);

    *out_len = FSO_IV_SIZE + (size_t)total + FSO_TAG_SIZE;

    EVP_CIPHER_CTX_free(ctx);
    return FSO_CRYPTO_OK;
}

int fso_aes_decrypt(const uint8_t *key,
                    const uint8_t *in, size_t in_len,
                    uint8_t *out, size_t *out_len)
{
    if (!key || !in || !out || !out_len) {
        return FSO_CRYPTO_ERR_INVALID;
    }

    if (in_len < FSO_IV_SIZE + FSO_TAG_SIZE) {
        return FSO_CRYPTO_ERR_INVALID;
    }

    /* Extraction de l'IV, du ciphertext et du tag. */
    const uint8_t *iv = in;
    const uint8_t *ct = in + FSO_IV_SIZE;
    size_t ct_len = in_len - FSO_IV_SIZE - FSO_TAG_SIZE;
    const uint8_t *tag = in + FSO_IV_SIZE + ct_len;

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return FSO_CRYPTO_ERR_DECRYPT;
    }

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return FSO_CRYPTO_ERR_DECRYPT;
    }

    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, FSO_IV_SIZE, NULL) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return FSO_CRYPTO_ERR_DECRYPT;
    }

    if (EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return FSO_CRYPTO_ERR_DECRYPT;
    }

    int len = 0;
    int total = 0;
    if (EVP_DecryptUpdate(ctx, out, &len, ct, (int)ct_len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return FSO_CRYPTO_ERR_DECRYPT;
    }
    total = len;

    /* Injection du tag pour vérification. */
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG,
                            FSO_TAG_SIZE, (void *)tag) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return FSO_CRYPTO_ERR_DECRYPT;
    }

    if (EVP_DecryptFinal_ex(ctx, out + total, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return FSO_CRYPTO_ERR_DECRYPT;
    }
    total += len;

    *out_len = (size_t)total;

    EVP_CIPHER_CTX_free(ctx);
    return FSO_CRYPTO_OK;
}