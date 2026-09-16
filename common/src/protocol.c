#include "protocol.h"

#include <string.h>

/* ============================================================
 * fsociety — Implémentation du protocole
 * ============================================================ */

/* ---------- Helpers endianness ---------- */

static void write_u32_be(uint8_t *out, uint32_t v)
{
    out[0] = (uint8_t)(v >> 24);
    out[1] = (uint8_t)(v >> 16);
    out[2] = (uint8_t)(v >> 8);
    out[3] = (uint8_t)(v);
}

static uint32_t read_u32_be(const uint8_t *in)
{
    return ((uint32_t)in[0] << 24) |
           ((uint32_t)in[1] << 16) |
           ((uint32_t)in[2] << 8)  |
           ((uint32_t)in[3]);
}

static void write_u16_be(uint8_t *out, uint16_t v)
{
    out[0] = (uint8_t)(v >> 8);
    out[1] = (uint8_t)(v);
}

static uint16_t read_u16_be(const uint8_t *in)
{
    return ((uint16_t)in[0] << 8) | ((uint16_t)in[1]);
}

/* ---------- Pack ---------- */

int fso_pack(uint8_t type, uint16_t seq_id,
             const uint8_t *payload, uint32_t payload_len,
             uint8_t *out, size_t out_size)
{
    if (!out) return -1;
    if (payload_len > FSO_MAX_PAYLOAD) return -1;
    if (payload_len > 0 && !payload) return -1;
    if (out_size < FSO_HEADER_SIZE + payload_len) return -1;

    /* MAGIC (4 octets, big-endian) */
    write_u32_be(out, FSO_MAGIC);

    /* TYPE (1 octet) */
    out[4] = type;

    /* SEQ_ID (2 octets, big-endian) */
    write_u16_be(out + 5, seq_id);

    /* LENGTH (4 octets, big-endian) */
    write_u32_be(out + 7, payload_len);

    /* PAYLOAD */
    if (payload_len > 0) {
        memcpy(out + FSO_HEADER_SIZE, payload, payload_len);
    }

    return (int)(FSO_HEADER_SIZE + payload_len);
}

/* ---------- Unpack ---------- */

int fso_unpack(const uint8_t *in, size_t in_size,
               fso_header_t *header, const uint8_t **payload)
{
    if (!in || !header || !payload) return -1;
    if (in_size < FSO_HEADER_SIZE) return -1;

    /* MAGIC */
    uint32_t magic = read_u32_be(in);
    if (magic != FSO_MAGIC) return -1;

    /* TYPE */
    uint8_t type = in[4];

    /* SEQ_ID */
    uint16_t seq_id = read_u16_be(in + 5);

    /* LENGTH */
    uint32_t length = read_u32_be(in + 7);

    if (length > FSO_MAX_PAYLOAD) return -1;
    if (in_size < FSO_HEADER_SIZE + length) return -1;

    header->magic = magic;
    header->type = type;
    header->seq_id = seq_id;
    header->length = length;

    *payload = (length > 0) ? (in + FSO_HEADER_SIZE) : NULL;

    return 0;
}

/* ---------- Build command ---------- */

int fso_build_cmd(uint8_t cmd_id, const char *args,
                  uint8_t *out, size_t out_size)
{
    if (!out) return -1;

    size_t args_len = args ? strlen(args) : 0;
    size_t total = 1 + args_len;

    if (out_size < total + 1) return -1;

    out[0] = cmd_id;
    if (args_len > 0) {
        memcpy(out + 1, args, args_len);
    }
    out[total] = '\0';

    return (int)total;
}

/* ---------- Build result ---------- */

int fso_build_result(uint8_t cmd_id, uint8_t status, const char *output,
                     uint8_t *out, size_t out_size)
{
    if (!out) return -1;

    size_t output_len = output ? strlen(output) : 0;
    size_t total = 2 + output_len;

    if (out_size < total + 1) return -1;

    out[0] = cmd_id;
    out[1] = status;
    if (output_len > 0) {
        memcpy(out + 2, output, output_len);
    }
    out[total] = '\0';

    return (int)total;
}

/* ---------- Build chunk ---------- */

int fso_build_chunk(uint32_t transfer_id, uint32_t chunk_index,
                    uint32_t total_chunks, const uint8_t *data,
                    uint16_t data_len, uint8_t *out, size_t out_size)
{
    if (!out) return -1;
    if (data_len > 0 && !data) return -1;

    /* En-tête du chunk : 4 + 4 + 4 + 2 = 14 octets */
    size_t header_len = 14;
    size_t total = header_len + data_len;

    if (out_size < total) return -1;

    write_u32_be(out + 0, transfer_id);
    write_u32_be(out + 4, chunk_index);
    write_u32_be(out + 8, total_chunks);
    write_u16_be(out + 12, data_len);

    if (data_len > 0) {
        memcpy(out + header_len, data, data_len);
    }

    return (int)total;
}