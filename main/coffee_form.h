#pragma once

#include <stdbool.h>
#include <stddef.h>

#define COFFEE_FORM_MAX_BODY 512
#define COFFEE_FORM_TOKEN_LENGTH 32

typedef struct {
    char ssid[33];
    char password[64];
} coffee_credentials_t;

/* Strict application/x-www-form-urlencoded; output is cleared on failure. */
bool coffee_form_parse(const char *body, size_t length, const char *token,
                       coffee_credentials_t *out);
bool coffee_credentials_valid(const coffee_credentials_t *credentials);
