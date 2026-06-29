/*
 * Standalone compilation unit for Base64 encode/decode functions.
 * Extracted from ElunaUtility.cpp for unit testing without game-server deps.
 */

#include "Common.h"
#include <cstring>
#include <string>

namespace ElunaUtil
{

static char encoding_table[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
                                'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
                                'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
                                'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
                                'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
                                'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
                                'w', 'x', 'y', 'z', '0', '1', '2', '3',
                                '4', '5', '6', '7', '8', '9', '+', '/'};
static char decoding_table[256];
static int mod_table[] = {0, 2, 1};

static void build_decoding_table()
{
    for (int i = 0; i < 64; i++)
        decoding_table[(unsigned char)encoding_table[i]] = i;
}

void EncodeData(const unsigned char* data, size_t input_length, std::string& output)
{
    size_t output_length = 4 * ((input_length + 2) / 3);
    char* buffer = new char[output_length];

    for (size_t i = 0, j = 0; i < input_length;)
    {
        uint32 octet_a = i < input_length ? (unsigned char)data[i++] : 0;
        uint32 octet_b = i < input_length ? (unsigned char)data[i++] : 0;
        uint32 octet_c = i < input_length ? (unsigned char)data[i++] : 0;

        uint32 triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;

        buffer[j++] = encoding_table[(triple >> (3 * 6)) & 0x3F];
        buffer[j++] = encoding_table[(triple >> (2 * 6)) & 0x3F];
        buffer[j++] = encoding_table[(triple >> (1 * 6)) & 0x3F];
        buffer[j++] = encoding_table[(triple >> (0 * 6)) & 0x3F];
    }

    for (int i = 0; i < mod_table[input_length % 3]; i++)
        buffer[output_length - 1 - i] = '=';

    output.assign(buffer, output_length);
    delete[] buffer;
}

unsigned char* DecodeData(const char *data, size_t *output_length)
{
    if (decoding_table[(unsigned char)'B'] == 0)
        build_decoding_table();

    size_t input_length = strlen(data);

    if (input_length % 4 != 0)
        return NULL;

    for (size_t i = 0; i < input_length; ++i)
    {
        unsigned char byte = data[i];

        if (byte == '=')
            continue;

        if (decoding_table[byte] == 0 && byte != 'A')
            return NULL;
    }

    *output_length = input_length / 4 * 3;
    if (data[input_length - 1] == '=') (*output_length)--;
    if (data[input_length - 2] == '=') (*output_length)--;

    unsigned char *decoded_data = new unsigned char[*output_length];
    if (!decoded_data)
        return NULL;

    for (size_t i = 0, j = 0; i < input_length;)
    {
        uint32 sextet_a = data[i] == '=' ? 0 & i++ : decoding_table[(unsigned char)data[i++]];
        uint32 sextet_b = data[i] == '=' ? 0 & i++ : decoding_table[(unsigned char)data[i++]];
        uint32 sextet_c = data[i] == '=' ? 0 & i++ : decoding_table[(unsigned char)data[i++]];
        uint32 sextet_d = data[i] == '=' ? 0 & i++ : decoding_table[(unsigned char)data[i++]];

        uint32 triple = (sextet_a << (3 * 6))
        + (sextet_b << (2 * 6))
        + (sextet_c << (1 * 6))
        + (sextet_d << (0 * 6));

        if (j < *output_length) decoded_data[j++] = (triple >> (2 * 8)) & 0xFF;
        if (j < *output_length) decoded_data[j++] = (triple >> (1 * 8)) & 0xFF;
        if (j < *output_length) decoded_data[j++] = (triple >> (0 * 8)) & 0xFF;
    }

    return decoded_data;
}

} // namespace ElunaUtil
