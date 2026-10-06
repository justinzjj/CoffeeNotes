#include "coffee_form.h"
#include <string.h>

bool coffee_credentials_valid(const coffee_credentials_t *credentials)
{
    if (!credentials) return false;
    const char *ssid_end = memchr(credentials->ssid, 0, sizeof(credentials->ssid));
    const char *password_end = memchr(credentials->password, 0, sizeof(credentials->password));
    if (!ssid_end || !password_end || ssid_end == credentials->ssid) return false;
    size_t n = (size_t)(password_end - credentials->password);
    return n == 0 || (n >= 8 && n <= 63);
}

static int hex(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static bool decode(const char *src, size_t length, char *dst, size_t capacity)
{
    size_t used = 0;
    for (size_t i = 0; i < length; ++i) {
        unsigned char c = (unsigned char)src[i];
        if (c == '%') {
            if (i + 2 >= length) return false;
            int high = hex(src[i+1]), low = hex(src[i+2]);
            if (high < 0 || low < 0) return false;
            c = (unsigned char)((high << 4) | low);
            i += 2;
        } else if (c == '+') c = ' ';
        if (!c || used + 1 >= capacity) return false;
        dst[used++] = (char)c;
    }
    dst[used] = 0;
    return true;
}

bool coffee_form_parse(const char *body, size_t length, const char *token,
                       coffee_credentials_t *out)
{
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    if (!body || !token || !length || length > COFFEE_FORM_MAX_BODY ||
        strlen(token) != COFFEE_FORM_TOKEN_LENGTH || memchr(body, 0, length)) return false;
    coffee_credentials_t parsed = {0};
    unsigned seen = 0;
    size_t start = 0;
    while (start < length) {
        size_t end = start;
        while (end < length && body[end] != '&') ++end;
        const char *equal = memchr(body + start, '=', end - start);
        if (!equal) return false;
        char key[16], value[64];
        size_t key_length = (size_t)(equal - body - start);
        if (!decode(body + start, key_length, key, sizeof(key)) ||
            !decode(equal + 1, end - start - key_length - 1, value, sizeof(value))) return false;
        unsigned bit;
        if (!strcmp(key, "ssid")) {
            bit = 1;
            if (strlen(value) > 32) return false;
            memcpy(parsed.ssid, value, strlen(value) + 1);
        } else if (!strcmp(key, "password")) {
            bit = 2;
            memcpy(parsed.password, value, strlen(value) + 1);
        } else if (!strcmp(key, "token")) {
            bit = 4;
            if (strcmp(value, token)) return false;
        } else return false;
        if (seen & bit) return false;
        seen |= bit;
        if (end < length && end + 1 == length) return false;
        start = end + 1;
    }
    if (seen != 7 || !coffee_credentials_valid(&parsed)) return false;
    *out = parsed;
    return true;
}
