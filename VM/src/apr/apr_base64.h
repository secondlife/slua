#pragma once

int apr_base64_decode_len(const char *bufcoded);
int apr_base64_decode_binary(unsigned char *bufplain,
                             const char *bufcoded);
int apr_base64_encode_len(int len);
int apr_base64_encode_binary(char *encoded,
                             const unsigned char *string, int len);
