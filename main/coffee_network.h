#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    COFFEE_NET_OFF = 0,
    COFFEE_NET_NEED_SETUP,
    COFFEE_NET_CONNECTING,
    COFFEE_NET_ONLINE,
    COFFEE_NET_SYNCED,
    COFFEE_NET_FAILED,
    COFFEE_NET_AP_READY,
    COFFEE_NET_TESTING,
    COFFEE_NET_SAVED,
    COFFEE_NET_STORE_FAILED,
} coffee_net_status_t;

typedef struct {
    coffee_net_status_t status;
    bool provisioning;
    bool connected;
    bool clock_valid;
    uint32_t sync_generation;
    uint64_t sync_monotonic_us;
    char ap_ssid[33];
    char ap_password[9];
} coffee_network_info_t;

/* Call init once during application startup. Commands return queue acceptance. */
bool coffee_network_init(void);
bool coffee_network_setup(void);
bool coffee_network_cancel(void);
bool coffee_network_forget(void);
void coffee_network_snapshot(coffee_network_info_t *out);
bool coffee_network_clock_valid(void);
