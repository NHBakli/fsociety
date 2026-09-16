#ifndef FSO_RSA_DER_H
#define FSO_RSA_DER_H

#include <stdint.h>
#include <stddef.h>
#include "crypto.h"

/* ============================================================
 * fsociety — Conversion RSA BCrypt ↔ DER
 * ============================================================ */

/* Convertit un blob BCrypt (BCRYPT_RSAPUBLIC_BLOB) en DER
 * SubjectPublicKeyInfo (format OpenSSL).
 *
 * `out` doit être alloué par l'appelant.
 * `out_len` reçoit la taille réelle.
 *
 * Retourne FSO_CRYPTO_OK ou un code d'erreur. */
int fso_rsa_bcrypt_to_der(const uint8_t *bcrypt_blob, size_t bcrypt_len,
                          uint8_t *out, size_t *out_len);

/* Convertit un DER SubjectPublicKeyInfo en blob BCrypt
 * (BCRYPT_RSAPUBLIC_BLOB).
 *
 * `out` doit être alloué par l'appelant.
 * `out_len` reçoit la taille réelle.
 *
 * Retourne FSO_CRYPTO_OK ou un code d'erreur. */
int fso_rsa_der_to_bcrypt(const uint8_t *der, size_t der_len,
                          uint8_t *out, size_t *out_len);

#endif /* FSO_RSA_DER_H */