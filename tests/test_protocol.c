#include "protocol.h"

#include <stdio.h>
#include <string.h>

static int test_pack_unpack(void)
{
    printf("[*] Test pack/unpack...\n");

    const char *msg = "hello";
    uint8_t payload[64];
    int payload_len = (int)strlen(msg);
    memcpy(payload, msg, payload_len);

    uint8_t packet[128];
    int packet_len = fso_pack(MSG_CMD, 42, payload, payload_len,
                              packet, sizeof(packet));
    if (packet_len < 0) {
        printf("[-] pack échoué\n");
        return -1;
    }
    printf("    Pack : %d octets\n", packet_len);

    fso_header_t header;
    const uint8_t *out_payload;
    if (fso_unpack(packet, packet_len, &header, &out_payload) != 0) {
        printf("[-] unpack échoué\n");
        return -1;
    }

    printf("    Type    : 0x%02X\n", header.type);
    printf("    Seq     : %u\n", header.seq_id);
    printf("    Length  : %u\n", header.length);

    if (header.type != MSG_CMD) return -1;
    if (header.seq_id != 42) return -1;
    if (header.length != (uint32_t)payload_len) return -1;
    if (memcmp(out_payload, msg, payload_len) != 0) return -1;

    printf("[+] pack/unpack OK\n\n");
    return 0;
}

static int test_build_cmd(void)
{
    printf("[*] Test build_cmd...\n");

    uint8_t payload[64];
    int len = fso_build_cmd(CMD_SHELL, "whoami",
                            payload, sizeof(payload));
    if (len < 0) {
        printf("[-] build_cmd échoué\n");
        return -1;
    }

    printf("    CMD_ID : 0x%02X\n", payload[0]);
    printf("    ARGS   : %s\n", (char *)(payload + 1));

    if (payload[0] != CMD_SHELL) return -1;
    if (strcmp((char *)(payload + 1), "whoami") != 0) return -1;

    printf("[+] build_cmd OK\n\n");
    return 0;
}

static int test_build_result(void)
{
    printf("[*] Test build_result...\n");

    uint8_t payload[128];
    int len = fso_build_result(CMD_SHELL, FSO_OK, "desktop-abc\\victim",
                               payload, sizeof(payload));
    if (len < 0) {
        printf("[-] build_result échoué\n");
        return -1;
    }

    printf("    CMD_ID : 0x%02X\n", payload[0]);
    printf("    STATUS : 0x%02X\n", payload[1]);
    printf("    OUTPUT : %s\n", (char *)(payload + 2));

    if (payload[0] != CMD_SHELL) return -1;
    if (payload[1] != FSO_OK) return -1;
    if (strcmp((char *)(payload + 2), "desktop-abc\\victim") != 0) return -1;

    printf("[+] build_result OK\n\n");
    return 0;
}

int main(void)
{
    printf("========================================\n");
    printf("  fsociety — Test protocole\n");
    printf("========================================\n\n");

    int ret = 0;
    if (test_pack_unpack() != 0) ret = 1;
    if (test_build_cmd() != 0) ret = 1;
    if (test_build_result() != 0) ret = 1;

    printf("========================================\n");
    if (ret == 0) {
        printf("  [+] Tous les tests sont passés\n");
    } else {
        printf("  [-] Certains tests ont échoué\n");
    }
    printf("========================================\n");

    return ret;
}