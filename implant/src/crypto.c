#include "crypto.h"

#include <windows.h>
#include <bcrypt.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "bcrypt.lib")

/* ============================================================
 * fsociety — Implémentation crypto (Windows BCrypt)
 * ============================================================ */

#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#endif

/* ---------- Init / cleanup ---------- */

int fso_crypto_init(void)
{
    /* BCrypt s'initialise à la première utilisation. */
    return FSO_CRYPTO_OK;
}

void fso_crypto_cleanup(void)
{
    /* Rien à faire. */
}

/* ---------- Aléatoire ---------- */

int fso_random_bytes(uint8_t *buf, size_t len)
{
    if (!buf || len == 0) {
        return FSO_CRYPTO_ERR_INVALID;
    }
    NTSTATUS st = BCryptGenRandom(NULL, buf, (ULONG)len,
                                  BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (!NT_SUCCESS(st)) {
        return FSO_CRYPTO_ERR_RANDOM;
    }
    return FSO_CRYPTO_OK;
}

/* ---------- RSA ---------- */

int fso_rsa_generate(fso_rsa_keypair_t *kp)
{
    if (!kp) return FSO_CRYPTO_ERR_INVALID;

    memset(kp, 0, sizeof(*kp));

    BCRYPT_ALG_HANDLE hAlg = NULL;
    NTSTATUS st = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_RSA_ALGORITHM,
                                              NULL, 0);
    if (!NT_SUCCESS(st)) return FSO_CRYPTO_ERR_INIT;

    BCRYPT_KEY_HANDLE hKey = NULL;
    st = BCryptGenerateKeyPair(hAlg, &hKey, FSO_RSA_KEY_BITS, 0);
    if (!NT_SUCCESS(st)) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return FSO_CRYPTO_ERR_KEYGEN;
    }

    st = BCryptFinalizeKeyPair(hKey, 0);
    if (!NT_SUCCESS(st)) {
        BCryptDestroyKey(hKey);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return FSO_CRYPTO_ERR_KEYGEN;
    }

    /* Export clé publique (DER SubjectPublicKeyInfo). */
    ULONG pub_len = 0;
    st = BCryptExportKey(hKey, NULL, BCRYPT_RSAPUBLIC_BLOB, NULL, 0,
                         &pub_len, 0);
    if (!NT_SUCCESS(st)) {
        BCryptDestroyKey(hKey);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return FSO_CRYPTO_ERR_KEYGEN;
    }

    uint8_t *pub_blob = malloc(pub_len);
    if (!pub_blob) {
        BCryptDestroyKey(hKey);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return FSO_CRYPTO_ERR_MEMORY;
    }

    st = BCryptExportKey(hKey, NULL, BCRYPT_RSAPUBLIC_BLOB,
                         pub_blob, pub_len, &pub_len, 0);
    if (!NT_SUCCESS(st)) {
        free(pub_blob);
        BCryptDestroyKey(hKey);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return FSO_CRYPTO_ERR_KEYGEN;
    }

    /* Export clé privée. */
    ULONG priv_len = 0;
    st = BCryptExportKey(hKey, NULL, BCRYPT_RSAFULLPRIVATE_BLOB,
                         NULL, 0, &priv_len, 0);
    if (!NT_SUCCESS(st)) {
        free(pub_blob);
        BCryptDestroyKey(hKey);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return FSO_CRYPTO_ERR_KEYGEN;
    }

    uint8_t *priv_blob = malloc(priv_len);
    if (!priv_blob) {
        free(pub_blob);
        BCryptDestroyKey(hKey);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return FSO_CRYPTO_ERR_MEMORY;
    }

    st = BCryptExportKey(hKey, NULL, BCRYPT_RSAFULLPRIVATE_BLOB,
                         priv_blob, priv_len, &priv_len, 0);
    if (!NT_SUCCESS(st)) {
        free(pub_blob);
        free(priv_blob);
        BCryptDestroyKey(hKey);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return FSO_CRYPTO_ERR_KEYGEN;
    }

    kp->pub = pub_blob;
    kp->pub_len = pub_len;
    kp->priv = priv_blob;
    kp->priv_len = priv_len;

    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return FSO_CRYPTO_OK;
}

int fso_rsa_encrypt(const uint8_t *pub, size_t pub_len,
                    const uint8_t *in, size_t in_len,
                    uint8_t *out, size_t *out_len)
{
    if (!pub || !in || !out || !out_len) return FSO_CRYPTO_ERR_INVALID;

    BCRYPT_ALG_HANDLE hAlg = NULL;
    NTSTATUS st = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_RSA_ALGORITHM,
                                              NULL, 0);
    if (!NT_SUCCESS(st)) return FSO_CRYPTO_ERR_INIT;

    BCRYPT_KEY_HANDLE hKey = NULL;
    st = BCryptImportKeyPair(hAlg, NULL, BCRYPT_RSAPUBLIC_BLOB, &hKey,
                             (PUCHAR)pub, (ULONG)pub_len, 0);
    if (!NT_SUCCESS(st)) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return FSO_CRYPTO_ERR_FORMAT;
    }

    BCRYPT_OAEP_PADDING_INFO pad = { 0 };
    pad.pszAlgId = BCRYPT_SHA256_ALGORITHM;
    pad.pbLabel = NULL;
    pad.cbLabel = 0;

    ULONG len = 0;
    st = BCryptEncrypt(hKey, (PUCHAR)in, (ULONG)in_len,
                       &pad, NULL, 0, out, (ULONG)*out_len, &len,
                       BCRYPT_PAD_OAEP);
    if (!NT_SUCCESS(st)) {
        BCryptDestroyKey(hKey);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return FSO_CRYPTO_ERR_ENCRYPT;
    }

    *out_len = len;

    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return FSO_CRYPTO_OK;
}

int fso_rsa_decrypt(const uint8_t *priv, size_t priv_len,
                    const uint8_t *in, size_t in_len,
                    uint8_t *out, size_t *out_len)
{
    if (!priv || !in || !out || !out_len) return FSO_CRYPTO_ERR_INVALID;

    BCRYPT_ALG_HANDLE hAlg = NULL;
    NTSTATUS st = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_RSA_ALGORITHM,
                                              NULL, 0);
    if (!NT_SUCCESS(st)) return FSO_CRYPTO_ERR_INIT;

    BCRYPT_KEY_HANDLE hKey = NULL;
    st = BCryptImportKeyPair(hAlg, NULL, BCRYPT_RSAFULLPRIVATE_BLOB, &hKey,
                             (PUCHAR)priv, (ULONG)priv_len, 0);
    if (!NT_SUCCESS(st)) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return FSO_CRYPTO_ERR_FORMAT;
    }

    BCRYPT_OAEP_PADDING_INFO pad = { 0 };
    pad.pszAlgId = BCRYPT_SHA256_ALGORITHM;

    ULONG len = 0;
    st = BCryptDecrypt(hKey, (PUCHAR)in, (ULONG)in_len,
                       &pad, NULL, 0, out, (ULONG)*out_len, &len,
                       BCRYPT_PAD_OAEP);
    if (!NT_SUCCESS(st)) {
        BCryptDestroyKey(hKey);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return FSO_CRYPTO_ERR_DECRYPT;
    }

    *out_len = len;

    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return FSO_CRYPTO_OK;
}

void fso_rsa_free(fso_rsa_keypair_t *kp)
{
    if (!kp) return;
    free(kp->pub);
    free(kp->priv);
    memset(kp, 0, sizeof(*kp));
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
    if (!key || !plaintext || !out || !out_len) return FSO_CRYPTO_ERR_INVALID;

    BCRYPT_ALG_HANDLE hAlg = NULL;
    NTSTATUS st = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM,
                                              NULL, 0);
    if (!NT_SUCCESS(st)) return FSO_CRYPTO_ERR_INIT;

    st = BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE,
                           (PUCHAR)BCRYPT_CHAIN_MODE_GCM,
                           sizeof(BCRYPT_CHAIN_MODE_GCM), 0);
    if (!NT_SUCCESS(st)) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return FSO_CRYPTO_ERR_INIT;
    }

    BCRYPT_KEY_HANDLE hKey = NULL;
    st = BCryptGenerateSymmetricKey(hAlg, &hKey, NULL, 0,
                                    (PUCHAR)key, FSO_AES_KEY_SIZE, 0);
    if (!NT_SUCCESS(st)) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return FSO_CRYPTO_ERR_ENCRYPT;
    }

    /* IV aléatoire. */
    uint8_t iv[FSO_IV_SIZE];
    if (fso_random_bytes(iv, FSO_IV_SIZE) != FSO_CRYPTO_OK) {
        BCryptDestroyKey(hKey);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return FSO_CRYPTO_ERR_RANDOM;
    }

    /* Copie de l'IV en tête de sortie. */
    memcpy(out, iv, FSO_IV_SIZE);

    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO auth = { 0 };
    BCRYPT_INIT_AUTH_MODE_INFO(auth);
    auth.pbNonce = iv;
    auth.cbNonce = FSO_IV_SIZE;
    auth.pbTag = out + FSO_IV_SIZE + pt_len;
    auth.cbTag = FSO_TAG_SIZE;

    ULONG len = 0;
    st = BCryptEncrypt(hKey,
                       (PUCHAR)plaintext, (ULONG)pt_len,
                       &auth, NULL, 0,
                       out + FSO_IV_SIZE, (ULONG)pt_len,
                       &len, 0);
    if (!NT_SUCCESS(st)) {
        BCryptDestroyKey(hKey);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return FSO_CRYPTO_ERR_ENCRYPT;
    }

    *out_len = FSO_IV_SIZE + len + FSO_TAG_SIZE;

    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return FSO_CRYPTO_OK;
}

int fso_aes_decrypt(const uint8_t *key,
                    const uint8_t *in, size_t in_len,
                    uint8_t *out, size_t *out_len)
{
    if (!key || !in || !out || !out_len) return FSO_CRYPTO_ERR_INVALID;
    if (in_len < FSO_IV_SIZE + FSO_TAG_SIZE) return FSO_CRYPTO_ERR_INVALID;

    BCRYPT_ALG_HANDLE hAlg = NULL;
    NTSTATUS st = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM,
                                              NULL, 0);
    if (!NT_SUCCESS(st)) return FSO_CRYPTO_ERR_INIT;

    st = BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE,
                           (PUCHAR)BCRYPT_CHAIN_MODE_GCM,
                           sizeof(BCRYPT_CHAIN_MODE_GCM), 0);
    if (!NT_SUCCESS(st)) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return FSO_CRYPTO_ERR_INIT;
    }

    BCRYPT_KEY_HANDLE hKey = NULL;
    st = BCryptGenerateSymmetricKey(hAlg, &hKey, NULL, 0,
                                    (PUCHAR)key, FSO_AES_KEY_SIZE, 0);
    if (!NT_SUCCESS(st)) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return FSO_CRYPTO_ERR_DECRYPT;
    }

    const uint8_t *iv = in;
    const uint8_t *ct = in + FSO_IV_SIZE;
    size_t ct_len = in_len - FSO_IV_SIZE - FSO_TAG_SIZE;
    const uint8_t *tag = in + FSO_IV_SIZE + ct_len;

    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO auth = { 0 };
    BCRYPT_INIT_AUTH_MODE_INFO(auth);
    auth.pbNonce = (PUCHAR)iv;
    auth.cbNonce = FSO_IV_SIZE;
    auth.pbTag = (PUCHAR)tag;
    auth.cbTag = FSO_TAG_SIZE;

    ULONG len = 0;
    st = BCryptDecrypt(hKey,
                       (PUCHAR)ct, (ULONG)ct_len,
                       &auth, NULL, 0,
                       out, (ULONG)ct_len,
                       &len, 0);
    if (!NT_SUCCESS(st)) {
        BCryptDestroyKey(hKey);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return FSO_CRYPTO_ERR_DECRYPT;
    }

    *out_len = len;

    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return FSO_CRYPTO_OK;
}