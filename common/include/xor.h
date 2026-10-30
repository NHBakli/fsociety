#ifndef FSOCIETY_XOR_H
#define FSOCIETY_XOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool xor_transform(const uint8_t *input,
    uint8_t *output,
    size_t length,
    const uint8_t *key,
    size_t key_length);

#endif