#include "../include/xor.h"

bool xor_transform(const uint8_t *input,
    uint8_t * output,
    size_t length,
    const uint8_t * key,
    size_t key_length)
{
    if (key == NULL || key_length == 0 || (length > 0 && (input == NULL || output == NULL)))
    {
        return false;
    }
    for (size_t i = 0; i < length; i++)
    {
        output[i] = input[i] ^ key [i % key_length];
    }

    return true;
}