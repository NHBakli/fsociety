#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "../include/xor.h"

int main(void)
{
    uint8_t input[] = {0x41};
    uint8_t key[] = {0x01};
    uint8_t output[] = {0x00};

    bool success = xor_transform(
        input, output, sizeof(input), key, sizeof(key)
    );

    assert(success);
    assert(output[0] == 0x40);

    uint8_t decoded[] = {0x00};
    bool decode_success = xor_transform(
        output, decoded, sizeof(output), key, sizeof(key)
    );

    assert(decode_success);
    assert(decoded[0] == input[0]);

    uint8_t binary_input[] = {0x00, 0x41, 0x42, 0xFF};
    uint8_t binary_key[] = {0x01, 0x02};
    uint8_t binary_output[sizeof(binary_input)] = {0};
    uint8_t binary_decoded[sizeof(binary_input)] = {0};
    uint8_t expected[] = {0x01, 0x43, 0x43, 0xFD};

    bool binary_success = xor_transform(
        binary_input, binary_output, sizeof(binary_input),
        binary_key, sizeof(binary_key)
    );
    assert(binary_success);

    for (size_t i = 0; i < sizeof(binary_input); i++)
    {
        assert(binary_output[i] == expected[i]);
    }

    bool binary_decode_success = xor_transform(
        binary_output, binary_decoded, sizeof(binary_output),
        binary_key, sizeof(binary_key)
    );
    assert(binary_decode_success);

    for (size_t i = 0; i < sizeof(binary_input); i++)
    {
        assert(binary_decoded[i] == binary_input[i]);
    }

    uint8_t varied_input[] = {
        0x00, 0xFF, 0x41, 0x00, 0x7F, 0x80, 0x20, 0x42, 0x01
    };
    uint8_t varied_key[] = {0x12, 0x34, 0x56};

    for (size_t length = 0; length <= sizeof(varied_input); length++)
    {
        uint8_t varied_output[sizeof(varied_input)] = {0};
        uint8_t varied_decoded[sizeof(varied_input)] = {0};

        bool encode_success = xor_transform(
            varied_input, varied_output, length,
            varied_key, sizeof(varied_key)
        );
        bool round_trip_success = xor_transform(
            varied_output, varied_decoded, length,
            varied_key, sizeof(varied_key)
        );

        assert(encode_success);
        assert(round_trip_success);

        for (size_t i = 0; i < length; i++)
        {
            assert(varied_decoded[i] == varied_input[i]);
        }
        for (size_t i = length; i < sizeof(varied_input); i++)
        {
            assert(varied_output[i] == 0);
            assert(varied_decoded[i] == 0);
        }
    }

    uint8_t in_place[] = {0x00, 0x41, 0xFF};
    uint8_t in_place_original[] = {0x00, 0x41, 0xFF};
    uint8_t in_place_expected[] = {0x01, 0x43, 0xFE};

    bool in_place_success = xor_transform(
        in_place, in_place, sizeof(in_place), binary_key, sizeof(binary_key)
    );
    assert(in_place_success);
    for (size_t i = 0; i < sizeof(in_place); i++)
    {
        assert(in_place[i] == in_place_expected[i]);
    }

    bool in_place_decode_success = xor_transform(
        in_place, in_place, sizeof(in_place), binary_key, sizeof(binary_key)
    );
    assert(in_place_decode_success);
    for (size_t i = 0; i < sizeof(in_place); i++)
    {
        assert(in_place[i] == in_place_original[i]);
    }

    assert(xor_transform(NULL, NULL, 0, binary_key, sizeof(binary_key)));

    uint8_t untouched = 0xAA;
    assert(!xor_transform(binary_input, &untouched, 1, NULL, 1));
    assert(!xor_transform(binary_input, &untouched, 1, binary_key, 0));
    assert(!xor_transform(NULL, &untouched, 1, binary_key, sizeof(binary_key)));
    assert(!xor_transform(binary_input, NULL, 1, binary_key, sizeof(binary_key)));
    assert(untouched == 0xAA);

    puts("All XOR tests passed");
    return 0;
}
