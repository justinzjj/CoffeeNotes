#include "coffee_form.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const char token[] = "0123456789abcdef0123456789abcdef";
static bool parse(const char *fields, coffee_credentials_t *out)
{
    char body[600];
    int n = snprintf(body, sizeof(body), "%s&token=%s", fields, token);
    assert(n > 0 && (size_t)n < sizeof(body));
    return coffee_form_parse(body, (size_t)n, token, out);
}

int main(void)
{
    coffee_credentials_t out;
    assert(parse("ssid=%E5%92%96%E5%95%A1+room&password=12345678", &out));
    assert(strcmp(out.ssid, "咖啡 room") == 0);
    assert(strcmp(out.password, "12345678") == 0);
    assert(parse("ssid=open&password=", &out));
    assert(out.password[0] == 0);
    assert(parse("ssid=a&password=123456789012345678901234567890123456789012345678901234567890123", &out));
    assert(strlen(out.password) == 63);
    assert(parse("ssid=12345678901234567890123456789012&password=12345678", &out));
    assert(!parse("ssid=123456789012345678901234567890123&password=12345678", &out));
    assert(!parse("ssid=&password=12345678", &out));
    assert(!parse("ssid=coffee&password=1234567", &out));
    assert(!parse("ssid=coffee&password=1234567890123456789012345678901234567890123456789012345678901234", &out));
    assert(!parse("ssid=%&password=12345678", &out));
    assert(!parse("ssid=%0&password=12345678", &out));
    assert(!parse("ssid=%GG&password=12345678", &out));
    assert(!parse("ssid=good%00evil&password=12345678", &out));
    assert(!parse("ssid=coffee&password=12345678%00", &out));
    assert(!parse("ssid=coffee&ssid=other&password=12345678", &out));
    assert(!parse("ssid=coffee&password=12345678&token=bad", &out));
    assert(!parse("ssid=coffee", &out));
    assert(!parse("password=12345678", &out));
    assert(!parse("ssid=coffee&password=12345678&extra=1", &out));
    assert(!parse("ssid=coffee&password=12345678&", &out));
    assert(!parse("ssid=coffee&password=12345678&&", &out));
    const char missing[] = "ssid=coffee&password=12345678";
    assert(!coffee_form_parse(missing, sizeof(missing)-1, token, &out));
    const char bad[] = "ssid=coffee&password=12345678&token=wrong";
    assert(!coffee_form_parse(bad, sizeof(bad)-1, token, &out));
    const char nul[] = "ssid=caf\0fe&password=12345678&token=0123456789abcdef0123456789abcdef";
    assert(!coffee_form_parse(nul, sizeof(nul)-1, token, &out));
    char large[513]; memset(large, 'a', sizeof(large));
    assert(!coffee_form_parse(large, sizeof(large), token, &out));
    assert(out.ssid[0] == 0 && out.password[0] == 0);
    coffee_credentials_t invalid;
    memset(&invalid, 'x', sizeof(invalid));
    assert(!coffee_credentials_valid(&invalid));
    invalid.ssid[32] = 0;
    assert(!coffee_credentials_valid(&invalid));
    puts("coffee form tests: PASS");
    return 0;
}
