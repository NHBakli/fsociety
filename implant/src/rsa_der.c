#include "rsa_der.h"

#include <windows.h>
#include <bcrypt.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * fsociety — Conversion RSA BCrypt ↔ DER
 * ============================================================
 *
 * Format DER SubjectPublicKeyInfo :
 *
 *   SEQUENCE {
 *     SEQUENCE {
 *       OID 1.2.840.113549.1.1.1  (rsaEncryption)
 *       NULL
 *     }
 *     BIT STRING {
 *       SEQUENCE {
 *         INTEGER modulus
 *         INTEGER publicExponent
 *       }
 *     }
 *   }
 */

/* ---------- Helpers ASN.1 DER ---------- */

/* Écrit un tag + longueur + valeur. */
static size_t der_write_tlv(uint8_t *out, uint8_t tag,
                            const uint8_t *value, size_t value_len)
{
    size_t pos = 0;
    out[pos++] = tag;

    if (value_len < 0x80) {
        out[pos++] = (uint8_t)value_len;
    } else if (value_len < 0x100) {
        out[pos++] = 0x81;
        out[pos++] = (uint8_t)value_len;
    } else if (value_len < 0x10000) {
        out[pos++] = 0x82;
        out[pos++] = (uint8_t)(value_len >> 8);
        out[pos++] = (uint8_t)(value_len & 0xFF);
    } else {
        out[pos++] = 0x83;
        out[pos++] = (uint8_t)(value_len >> 16);
        out[pos++] = (uint8_t)((value_len >> 8) & 0xFF);
        out[pos++] = (uint8_t)(value_len & 0xFF);
    }

    if (value && value_len > 0) {
        memcpy(out + pos, value, value_len);
        pos += value_len;
    }

    return pos;
}

/* Calcule la taille d'un TLV. */
static size_t der_tlv_size(size_t value_len)
{
    size_t len_size = 1;
    if (value_len >= 0x80 && value_len < 0x100) len_size = 2;
    else if (value_len >= 0x100 && value_len < 0x10000) len_size = 3;
    else if (value_len >= 0x10000) len_size = 4;
    return 1 + len_size + value_len;
}

/* Ajoute un octet 0x00 en tête si le bit de poids fort est à 1
 * (pour les INTEGER signés). */
static size_t der_int_pad(const uint8_t *in, size_t in_len,
                          uint8_t *out)
{
    if (in_len > 0 && (in[0] & 0x80)) {
        out[0] = 0x00;
        memcpy(out + 1, in, in_len);
        return in_len + 1;
    }
    memcpy(out, in, in_len);
    return in_len;
}

/* ---------- BCrypt → DER ---------- */

int fso_rsa_bcrypt_to_der(const uint8_t *bcrypt_blob, size_t bcrypt_len,
                          uint8_t *out, size_t *out_len)
{
    if (!bcrypt_blob || !out || !out_len) return FSO_CRYPTO_ERR_INVALID;
    if (bcrypt_len < sizeof(BCRYPT_RSAKEY_BLOB)) return FSO_CRYPTO_ERR_FORMAT;

    const BCRYPT_RSAKEY_BLOB *blob = (const BCRYPT_RSAKEY_BLOB *)bcrypt_blob;

    if (blob->Magic != BCRYPT_RSAPUBLIC_MAGIC) {
        return FSO_CRYPTO_ERR_FORMAT;
    }

    const uint8_t *p = bcrypt_blob + sizeof(BCRYPT_RSAKEY_BLOB);
    const uint8_t *pub_exp = p;
    size_t pub_exp_len = blob->cbPublicExp;
    const uint8_t *modulus = p + pub_exp_len;
    size_t modulus_len = blob->cbModulus;

    /* Encodage des INTEGER avec padding. */
    uint8_t modulus_padded[512];
    uint8_t pub_exp_padded[512];

    size_t mod_pad_len = der_int_pad(modulus, modulus_len, modulus_padded);
    size_t exp_pad_len = der_int_pad(pub_exp, pub_exp_len, pub_exp_padded);

    /* RSAPublicKey ::= SEQUENCE { modulus INTEGER, publicExponent INTEGER } */
    uint8_t rsa_pub[1024];
    size_t pos = 0;
    pos += der_write_tlv(rsa_pub + pos, 0x02, modulus_padded, mod_pad_len);
    pos += der_write_tlv(rsa_pub + pos, 0x02, pub_exp_padded, exp_pad_len);
    size_t rsa_pub_len = pos;

    /* Englober RSAPublicKey dans une SEQUENCE. */
    uint8_t rsa_pub_seq[1024];
    size_t rsa_pub_seq_len = der_write_tlv(rsa_pub_seq, 0x30,
                                            rsa_pub, rsa_pub_len);

    /* AlgorithmIdentifier ::= SEQUENCE { OID rsaEncryption, NULL } */
    const uint8_t oid_rsa[] = {
        0x06, 0x09, 0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x01
    };
    const uint8_t null_param[] = { 0x05, 0x00 };

    uint8_t alg_id[32];
    alg_id[0] = 0x30;
    alg_id[1] = (uint8_t)(sizeof(oid_rsa) + sizeof(null_param));
    memcpy(alg_id + 2, oid_rsa, sizeof(oid_rsa));
    memcpy(alg_id + 2 + sizeof(oid_rsa), null_param, sizeof(null_param));
    size_t alg_id_len = 2 + sizeof(oid_rsa) + sizeof(null_param);

    /* BIT STRING contenant RSAPublicKey SEQUENCE. */
    uint8_t bit_string[1024];
    bit_string[0] = 0x00; /* unused bits */
    memcpy(bit_string + 1, rsa_pub_seq, rsa_pub_seq_len);
    size_t bit_string_len = 1 + rsa_pub_seq_len;

    uint8_t bit_string_tlv[1024];
    size_t bit_string_tlv_len = der_write_tlv(bit_string_tlv, 0x03,
                                              bit_string, bit_string_len);

    /* SubjectPublicKeyInfo ::= SEQUENCE { AlgorithmIdentifier, BIT STRING } */
    uint8_t spki_inner[2048];
    size_t spki_inner_len = 0;
    memcpy(spki_inner + spki_inner_len, alg_id, alg_id_len);
    spki_inner_len += alg_id_len;
    memcpy(spki_inner + spki_inner_len, bit_string_tlv, bit_string_tlv_len);
    spki_inner_len += bit_string_tlv_len;

    /* En-tête SEQUENCE externe. */
    size_t final_len = der_write_tlv(out, 0x30, spki_inner, spki_inner_len);

    *out_len = final_len;
    return FSO_CRYPTO_OK;
}

/* ---------- DER → BCrypt ---------- */

/* Parse un TLV DER. Retourne la position après le TLV. */
static size_t der_read_tlv(const uint8_t *in, size_t in_len,
                           uint8_t *tag, const uint8_t **value,
                           size_t *value_len)
{
    if (in_len < 2) return 0;
    size_t pos = 0;
    *tag = in[pos++];

    size_t len = in[pos++];
    if (len & 0x80) {
        size_t num_bytes = len & 0x7F;
        if (num_bytes > 4 || pos + num_bytes > in_len) return 0;
        len = 0;
        for (size_t i = 0; i < num_bytes; i++) {
            len = (len << 8) | in[pos++];
        }
    }

    if (pos + len > in_len) return 0;
    *value = in + pos;
    *value_len = len;
    return pos + len;
}

int fso_rsa_der_to_bcrypt(const uint8_t *der, size_t der_len,
                          uint8_t *out, size_t *out_len)
{
    if (!der || !out || !out_len) return FSO_CRYPTO_ERR_INVALID;

    /* Parse SEQUENCE externe (SubjectPublicKeyInfo). */
    uint8_t tag;
    const uint8_t *spki;
    size_t spki_len;
    if (der_read_tlv(der, der_len, &tag, &spki, &spki_len) == 0) {
        return FSO_CRYPTO_ERR_FORMAT;
    }
    if (tag != 0x30) return FSO_CRYPTO_ERR_FORMAT;

    /* Parse AlgorithmIdentifier. */
    const uint8_t *alg;
    size_t alg_len;
    size_t pos = der_read_tlv(spki, spki_len, &tag, &alg, &alg_len);
    if (pos == 0 || tag != 0x30) return FSO_CRYPTO_ERR_FORMAT;

    /* Parse BIT STRING. */
    const uint8_t *bit;
    size_t bit_len;
    pos = der_read_tlv(spki + pos, spki_len - pos, &tag, &bit, &bit_len);
    if (pos == 0 || tag != 0x03) return FSO_CRYPTO_ERR_FORMAT;

    /* Ignorer l'octet unused bits. */
    if (bit_len < 1) return FSO_CRYPTO_ERR_FORMAT;
    bit++;
    bit_len--;

    /* Parse RSAPublicKey SEQUENCE. */
    const uint8_t *rsa_seq;
    size_t rsa_seq_len;
    if (der_read_tlv(bit, bit_len, &tag, &rsa_seq, &rsa_seq_len) == 0) {
        return FSO_CRYPTO_ERR_FORMAT;
    }
    if (tag != 0x30) return FSO_CRYPTO_ERR_FORMAT;

    /* Parse modulus INTEGER. */
    const uint8_t *modulus;
    size_t modulus_len;
    pos = der_read_tlv(rsa_seq, rsa_seq_len, &tag, &modulus, &modulus_len);
    if (pos == 0 || tag != 0x02) return FSO_CRYPTO_ERR_FORMAT;

    /* Parse publicExponent INTEGER. */
    const uint8_t *pub_exp;
    size_t pub_exp_len;
    der_read_tlv(rsa_seq + pos, rsa_seq_len - pos, &tag, &pub_exp, &pub_exp_len);
    if (tag != 0x02) return FSO_CRYPTO_ERR_FORMAT;

    /* Retirer le padding 0x00 si présent. */
    if (modulus_len > 0 && modulus[0] == 0x00) {
        modulus++;
        modulus_len--;
    }
    if (pub_exp_len > 0 && pub_exp[0] == 0x00) {
        pub_exp++;
        pub_exp_len--;
    }

    /* Construire le blob BCrypt. */
    BCRYPT_RSAKEY_BLOB blob = { 0 };
    blob.Magic = BCRYPT_RSAPUBLIC_MAGIC;
    blob.BitLength = (ULONG)(modulus_len * 8);
    blob.cbPublicExp = (ULONG)pub_exp_len;
    blob.cbModulus = (ULONG)modulus_len;

    size_t total = sizeof(blob) + pub_exp_len + modulus_len;
    if (*out_len < total) return FSO_CRYPTO_ERR_MEMORY;

    memcpy(out, &blob, sizeof(blob));
    memcpy(out + sizeof(blob), pub_exp, pub_exp_len);
    memcpy(out + sizeof(blob) + pub_exp_len, modulus, modulus_len);

    *out_len = total;
    return FSO_CRYPTO_OK;
}