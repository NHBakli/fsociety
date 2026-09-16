#include "crypto.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    printf("fsociety implant crypto test\n");

    uint8_t key[FSO_AES_KEY_SIZE];
    fso_aes_generate_key(key);

    const char *msg = "Hello from Windows";
    uint8_t enc[256];
    size_t enc_len;
    fso_aes_encrypt(key, (const uint8_t *)msg, strlen(msg), enc, &enc_len);
    printf("Encrypted: %zu bytes\n", enc_len);

    uint8_t dec[256];
    size_t dec_len;
    fso_aes_decrypt(key, enc, enc_len, dec, &dec_len);
    dec[dec_len] = 0;
    printf("Decrypted: %s\n", dec);

    return 0;
}