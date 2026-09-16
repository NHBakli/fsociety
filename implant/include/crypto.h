#ifndef FSO_IMPLANT_CRYPTO_H
#define FSO_IMPLANT_CRYPTO_H

#include <stdint.h>
#include <stddef.h>

/* ============================================================
 * fsociety — Crypto côté implant (Windows BCrypt)
 * ============================================================ */

#define FSO_RSA_KEY_BITS   2048
#define FSO_AES_KEY_SIZE   32
#define FSO_IV_SIZE        12
#define FSO_TAG_SIZE       16

typedef enum {
    FSO_CRYPTO_OK          = 0,
    FSO_CRYPTO_ERR_INIT    = -1,
    FSO_CRYPTO_ERR_KEYGEN  = -2,
    FSO_CRYPTO_ERR_ENCRYPT = -3,
    FSO_CRYPTO_ERR_DECRYPT = -4,
    FSO_CRYPTO_ERR_RANDOM  = -5,
    FSO_CRYPTO_ERR_INVALID = -6,
    FSO_CRYPTO_ERR_MEMORY  = -7,
    FSO_CRYPTO_ERR_FORMAT  = -8
} fso_crypto_status_t;

typedef struct {
    uint8_t *pub;
    size_t   pub_len;
    uint8_t *priv;
    size_t   priv_len;
} fso_rsa_keypair_t;

/* ---------- Init ---------- */
int  fso_crypto_init(void);
void fso_crypto_cleanup(void);

/* ---------- Aléatoire ---------- */
int fso_random_bytes(uint8_t *buf, size_t len);

/* ---------- RSA ---------- */
int  fso_rsa_generate(fso_rsa_keypair_t *kp);
int  fso_rsa_encrypt(const uint8_t *pub, size_t pub_len,
                     const uint8_t *in, size_t in_len,
                     uint8_t *out, size_t *out_len);
int  fso_rsa_decrypt(const uint8_t *priv, size_t priv_len,
                     const uint8_t *in, size_t in_len,
                     uint8_t *out, size_t *out_len);
void fso_rsa_free(fso_rsa_keypair_t *kp);

/* ---------- AES ---------- */
int fso_aes_generate_key(uint8_t *key);
int fso_aes_encrypt(const uint8_t *key,
                    const uint8_t *plaintext, size_t pt_len,
                    uint8_t *out, size_t *out_len);
int fso_aes_decrypt(const uint8_t *key,
                    const uint8_t *in, size_t in_len,
                    uint8_t *out, size_t *out_len);

#endif /* FSO_IMPLANT_CRYPTO_H */