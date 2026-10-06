#include "coffee_network.h"
#include "coffee_form.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

typedef int esp_err_t;
typedef void *httpd_handle_t;
typedef void *esp_netif_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_WIFI_NOT_CONNECT -2
#define ESP_ERR_INVALID_STATE -3
#define WIFI_MODE_STA 1
#define WIFI_MODE_APSTA 3
#define WIFI_IF_STA 0
#define WIFI_AUTH_OPEN 0
#define WIFI_AUTH_WPA2_PSK 3
#define LINK_UP 1
#define LINK_DOWN 2
#define ATTEMPT_US 25000000LL
#define COFFEE_FORM_TOKEN_LENGTH 32
#define portENTER_CRITICAL(lock) ((void)(lock))
#define portEXIT_CRITICAL(lock) ((void)(lock))
typedef struct {
    struct {
        uint8_t ssid[32], password[64];
        struct { int authmode; } threshold;
        struct { bool capable, required; } pmf_cfg;
    } sta;
} wifi_config_t;

static int s_lock, s_link_events;
static coffee_network_info_t s_info;
static char s_token[33];
static bool s_candidate_pending, s_ap_active, s_started, s_attempt, s_have_saved;
static coffee_credentials_t s_saved;
static httpd_handle_t s_http;
static esp_netif_t *s_ap;
static int64_t s_cleanup_at, s_ap_stop_at, s_session_deadline, s_attempt_deadline, s_retry_at;
static int64_t now_us = 1000000;
static int configured_mode, mode_failures, stop_result, start_calls, destroy_calls;

static void status(coffee_net_status_t value) { s_info.status = value; }
static void log_error(const char *action, esp_err_t err) { (void)action; (void)err; }
static int64_t esp_timer_get_time(void) { return now_us; }
static esp_err_t httpd_stop(httpd_handle_t handle) { (void)handle; return ESP_OK; }
static esp_err_t esp_wifi_set_mode(int mode)
{
    if (mode_failures > 0) { --mode_failures; return ESP_FAIL; }
    configured_mode = mode;
    return ESP_OK;
}
static esp_err_t esp_wifi_stop(void) { return stop_result; }
static void esp_netif_destroy_default_wifi(void *netif)
{
    assert(netif);
    assert(!s_started || configured_mode == WIFI_MODE_STA);
    ++destroy_calls;
}
static esp_err_t esp_wifi_disconnect(void) { return ESP_OK; }
static void xEventGroupClearBits(int group, int bits) { (void)group; (void)bits; }
static esp_err_t esp_wifi_set_config(int interface, wifi_config_t *config)
{
    (void)interface; (void)config; return ESP_OK;
}
static esp_err_t esp_wifi_start(void) { ++start_calls; return ESP_OK; }
static esp_err_t esp_wifi_connect(void) { return ESP_OK; }

/* The runner inserts actual production function bodies at this marker. */
/* COFFEE_NETWORK_FUNCTIONS */

static void reset(void)
{
    memset(&s_info, 0, sizeof(s_info));
    memset(s_token, 0, sizeof(s_token));
    s_http = NULL;
    s_ap = (esp_netif_t *)(uintptr_t)1;
    s_ap_active = true;
    s_started = true;
    s_attempt = false;
    s_have_saved = true;
    memset(&s_saved, 0, sizeof(s_saved));
    memcpy(s_saved.ssid, "coffee", 7);
    s_cleanup_at = s_ap_stop_at = s_session_deadline = s_retry_at = 0;
    configured_mode = WIFI_MODE_APSTA;
    mode_failures = 0;
    stop_result = ESP_OK;
    start_calls = destroy_calls = 0;
}

int main(void)
{
    reset();
    mode_failures = 2; /* Both live and stopped mode changes fail. */
    close_ap();
    assert(!s_started);
    assert(s_ap_active); /* AP mode ownership survives a successful radio stop. */
    assert(s_ap != NULL && destroy_calls == 0 && s_cleanup_at > now_us);
    saved_connect();
    assert(start_calls == 0 && configured_mode == WIFI_MODE_APSTA);
    mode_failures = 0;
    close_ap();
    assert(!s_ap_active && s_ap == NULL && destroy_calls == 1 && s_cleanup_at == 0);
    saved_connect();
    assert(start_calls == 1 && configured_mode == WIFI_MODE_STA);

    reset();
    mode_failures = 1; /* Stopped mode change succeeds after live failure. */
    close_ap();
    assert(!s_ap_active && !s_ap && configured_mode == WIFI_MODE_STA);
    saved_connect();
    assert(start_calls == 1 && configured_mode == WIFI_MODE_STA);

    reset();
    mode_failures = 1;
    stop_result = ESP_FAIL;
    close_ap();
    assert(s_ap_active && s_ap && s_started && destroy_calls == 0);
    saved_connect();
    assert(start_calls == 0);

    reset();
    s_ap_active = false;
    s_started = false;
    s_ap = NULL;
    saved_connect();
    assert(start_calls == 1 && configured_mode == WIFI_MODE_STA);

    reset();
    s_ap_active = false;
    s_started = false;
    s_ap = NULL;
    mode_failures = 1;
    saved_connect();
    assert(start_calls == 0 && configured_mode == WIFI_MODE_APSTA);
    assert(s_info.status == COFFEE_NET_FAILED);

    reset();
    s_info.provisioning = true;
    saved_connect();
    assert(configured_mode == WIFI_MODE_APSTA); /* Live provisioning remains APSTA. */
    struct timeval sync = {.tv_sec = 1700000000};
    now_us = 2000000;
    time_synced(&sync);
    assert(s_info.clock_valid && s_info.sync_generation == 1);
    assert(s_info.sync_monotonic_us == (uint64_t)now_us);
    uint64_t manual_applied_us = 3000000;
    now_us = 4000000; /* A delayed worker cannot relabel this older sync. */
    assert(s_info.sync_monotonic_us < manual_applied_us);
    time_synced(&sync);
    assert(s_info.sync_generation == 2 && s_info.sync_monotonic_us > manual_applied_us);
    sync.tv_sec = 1577836799; /* Before 2020. */
    time_synced(&sync);
    assert(!s_info.clock_valid && s_info.sync_generation == 2);
    sync.tv_sec = 4102444800LL; /* 2100 is outside the supported range. */
    time_synced(&sync);
    assert(!s_info.clock_valid && s_info.sync_generation == 2);
    sync.tv_sec = 1577836800; /* Inclusive 2020 boundary. */
    time_synced(&sync);
    assert(s_info.clock_valid && s_info.sync_generation == 3);
    puts("coffee network cleanup and clock tests: PASS");
    return 0;
}
