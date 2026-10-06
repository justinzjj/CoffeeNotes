#include "coffee_network.h"
#include "coffee_form.h"

#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#define SESSION_US (300LL * 1000000)
#define ATTEMPT_US (25LL * 1000000)
#define LINK_UP BIT0
#define LINK_DOWN BIT1
#define CREDENTIAL_MAGIC 0x43465731u

static const char *TAG = "coffee_net";
typedef enum { CMD_SETUP, CMD_CANCEL, CMD_FORGET, CMD_CANDIDATE } command_kind_t;
typedef struct {
    command_kind_t kind;
    coffee_credentials_t credentials;
    char token[COFFEE_FORM_TOKEN_LENGTH + 1];
} command_t;
typedef struct {
    uint32_t magic;
    uint32_t version;
    coffee_credentials_t credentials;
} credential_blob_t;

/* Only the worker owns Wi-Fi, NVS, netifs and the HTTP server lifecycle. */
static QueueHandle_t s_commands;
static EventGroupHandle_t s_link_events;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static coffee_network_info_t s_info;
static char s_token[COFFEE_FORM_TOKEN_LENGTH + 1];
static bool s_candidate_pending;
static bool s_init_claimed;
static esp_netif_t *s_sta, *s_ap;
static httpd_handle_t s_http;
static esp_event_handler_instance_t s_wifi_handler, s_ip_handler;
static bool s_wifi_registered, s_ip_registered, s_wifi_initialized, s_started;
static bool s_sntp_initialized;
static bool s_have_saved, s_testing, s_attempt, s_ap_active;
static coffee_credentials_t s_saved, s_candidate;
static int64_t s_session_deadline, s_attempt_deadline, s_retry_at, s_ap_stop_at;
static int64_t s_cleanup_at;
static unsigned s_retries;

static void status(coffee_net_status_t value)
{
    portENTER_CRITICAL(&s_lock);
    s_info.status = value;
    portEXIT_CRITICAL(&s_lock);
}

void coffee_network_snapshot(coffee_network_info_t *out)
{
    if (!out) return;
    portENTER_CRITICAL(&s_lock);
    *out = s_info;
    portEXIT_CRITICAL(&s_lock);
}

bool coffee_network_clock_valid(void)
{
    coffee_network_info_t info;
    coffee_network_snapshot(&info);
    return info.clock_valid;
}

static bool enqueue(command_kind_t kind)
{
    command_t cmd = {.kind = kind};
    return s_commands && xQueueSend(s_commands, &cmd, 0) == pdTRUE;
}
bool coffee_network_setup(void) { return enqueue(CMD_SETUP); }
bool coffee_network_cancel(void) { return enqueue(CMD_CANCEL); }
bool coffee_network_forget(void) { return enqueue(CMD_FORGET); }

static void log_error(const char *action, esp_err_t err)
{
    if (err != ESP_OK) ESP_LOGW(TAG, "%s: %s", action, esp_err_to_name(err));
}

static void event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)data;
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP)
        xEventGroupSetBits(s_link_events, LINK_UP);
    else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED)
        xEventGroupSetBits(s_link_events, LINK_DOWN);
}

/* No callbacks touch UI, storage or Wi-Fi configuration. */
static esp_err_t reply(httpd_req_t *req, const char *code, const char *text)
{
    esp_err_t err = httpd_resp_set_status(req, code);
    if (err == ESP_OK) err = httpd_resp_set_type(req, "text/plain; charset=utf-8");
    if (err == ESP_OK) err = httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    if (err == ESP_OK) err = httpd_resp_send(req, text, HTTPD_RESP_USE_STRLEN);
    return err;
}

static const char PAGE[] =
    "<!doctype html><html lang=zh-CN><meta charset=utf-8>"
    "<meta name=viewport content='width=device-width,initial-scale=1'>"
    "<title>CoffeeNotes 配网</title><style>body{font:18px sans-serif;max-width:480px;"
    "margin:32px auto;padding:20px;background:#f6f0e6;color:#352b24}input,button{"
    "box-sizing:border-box;width:100%%;padding:14px;margin:10px 0;font-size:18px}"
    "button{background:#715541;color:white;border:0;border-radius:8px}</style>"
    "<h1>CoffeeNotes</h1><p>连接 2.4 GHz Wi-Fi，自动同步北京时间。"
    "此配网热点将在五分钟后关闭。</p><form id=f>"
    "<label>Wi-Fi 名称<input name=ssid required autocomplete=off></label>"
    "<label>Wi-Fi 密码<input name=password type=password autocomplete=off></label>"
    "<input type=hidden name=token value='%s'><button>测试并保存</button></form>"
    "<p id=m>开放网络可留空密码。密码为 8–63 字节。</p>"
    "<script>let m=document.querySelector('#m'),f=document.querySelector('#f'),timer;"
    "f.onsubmit=async e=>{e.preventDefault();f.querySelector('button').disabled=true;"
    "try{let r=await fetch('/configure',{method:'POST',body:new URLSearchParams(new FormData(f))});"
    "m.textContent=await r.text();if(r.status===202){clearInterval(timer);"
    "timer=setInterval(async()=>{try{let s=await(await fetch('/status')).json();"
    "m.textContent=s.message;if(s.done){clearInterval(timer);"
    "f.querySelector('button').disabled=false}}catch(e){clearInterval(timer);"
    "m.textContent='热点已关闭，请在设备上确认已保存状态。'}},1000)}"
    "else f.querySelector('button').disabled=false}catch(e){m.textContent='发送失败，请重试';"
    "f.querySelector('button').disabled=false}};</script></html>";

static esp_err_t page_get(httpd_req_t *req)
{
    char token[sizeof(s_token)];
    portENTER_CRITICAL(&s_lock);
    memcpy(token, s_token, sizeof(token));
    portEXIT_CRITICAL(&s_lock);
    if (!token[0]) return reply(req, "410 Gone", "配网已结束");
    char *page = malloc(sizeof(PAGE) + sizeof(token));
    if (!page) return reply(req, "503 Service Unavailable", "内存不足，请重试");
    int n = snprintf(page, sizeof(PAGE) + sizeof(token), PAGE, token);
    esp_err_t err = ESP_FAIL;
    if (n > 0 && (size_t)n < sizeof(PAGE) + sizeof(token)) {
        err = httpd_resp_set_type(req, "text/html; charset=utf-8");
        if (err == ESP_OK) err = httpd_resp_set_hdr(req, "Cache-Control", "no-store");
        if (err == ESP_OK) err = httpd_resp_send(req, page, n);
    }
    free(page);
    return err;
}

static esp_err_t status_get(httpd_req_t *req)
{
    coffee_network_info_t info;
    coffee_network_snapshot(&info);
    bool done = info.status == COFFEE_NET_SAVED || info.status == COFFEE_NET_STORE_FAILED ||
                info.status == COFFEE_NET_FAILED;
    const char *message = "正在验证连接，尚未保存";
    if (info.status == COFFEE_NET_SAVED) message = "连接验证成功，已保存。热点即将关闭";
    else if (info.status == COFFEE_NET_STORE_FAILED) message = "连接成功但保存失败，请在设备上重试";
    else if (info.status == COFFEE_NET_FAILED) message = "连接验证失败，未更改已保存的网络，请重试";
    char json[256];
    snprintf(json, sizeof(json), "{\"done\":%s,\"message\":\"%s\"}", done ? "true" : "false", message);
    esp_err_t err = httpd_resp_set_type(req, "application/json; charset=utf-8");
    if (err == ESP_OK) err = httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    if (err == ESP_OK) err = httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    return err;
}

static esp_err_t configure_post(httpd_req_t *req)
{
    if (!req->content_len || req->content_len > COFFEE_FORM_MAX_BODY)
        return reply(req, "400 Bad Request", "表单长度不合法");
    char content_type[64];
    static const char media_type[] = "application/x-www-form-urlencoded";
    if (httpd_req_get_hdr_value_str(req, "Content-Type", content_type, sizeof(content_type)) != ESP_OK ||
        strncasecmp(content_type, media_type, sizeof(media_type)-1) ||
        (content_type[sizeof(media_type)-1] != 0 && content_type[sizeof(media_type)-1] != ';'))
        return reply(req, "415 Unsupported Media Type", "请使用配网页面提交");
    char body[COFFEE_FORM_MAX_BODY + 1];
    size_t received = 0;
    while (received < req->content_len) {
        int n = httpd_req_recv(req, body + received, req->content_len - received);
        /* Closing this socket on timeout/disconnect avoids reusing a partial body. */
        if (n <= 0) return ESP_FAIL;
        received += (size_t)n;
    }
    body[received] = 0;
    command_t cmd = {.kind = CMD_CANDIDATE};
    portENTER_CRITICAL(&s_lock);
    bool active = s_info.provisioning;
    memcpy(cmd.token, s_token, sizeof(cmd.token));
    portEXIT_CRITICAL(&s_lock);
    if (!active || !coffee_form_parse(body, received, cmd.token, &cmd.credentials)) {
        memset(body, 0, sizeof(body));
        return reply(req, "400 Bad Request", "名称、密码或配网令牌不合法");
    }
    memset(body, 0, sizeof(body));
    portENTER_CRITICAL(&s_lock);
    bool busy = s_candidate_pending;
    if (!busy) s_candidate_pending = true;
    portEXIT_CRITICAL(&s_lock);
    if (busy) return reply(req, "409 Conflict", "正在验证，请稍候");
    if (xQueueSend(s_commands, &cmd, 0) != pdTRUE) {
        portENTER_CRITICAL(&s_lock);
        s_candidate_pending = false;
        portEXIT_CRITICAL(&s_lock);
        memset(&cmd, 0, sizeof(cmd));
        return reply(req, "503 Service Unavailable", "设备忙，请重试");
    }
    memset(&cmd, 0, sizeof(cmd));
    return reply(req, "202 Accepted", "已接收，正在验证连接；验证成功后才会保存");
}

static void close_ap(void)
{
    /* Invalidate the token before waiting for the HTTP task to stop. */
    portENTER_CRITICAL(&s_lock);
    memset(s_token, 0, sizeof(s_token));
    s_info.provisioning = false;
    s_candidate_pending = false;
    memset(s_info.ap_password, 0, sizeof(s_info.ap_password));
    memset(s_info.ap_ssid, 0, sizeof(s_info.ap_ssid));
    portEXIT_CRITICAL(&s_lock);
    if (s_http) {
        esp_err_t err = httpd_stop(s_http);
        log_error("HTTP stop", err);
        if (err == ESP_OK) s_http = NULL;
    }
    if (s_ap_active) {
        esp_err_t err = esp_wifi_set_mode(WIFI_MODE_STA);
        log_error("AP stop", err);
        if (err != ESP_OK) {
            /* Stop all radio activity if AP disable failed; do not leave the AP open. */
            esp_err_t stop_err = esp_wifi_stop();
            log_error("radio stop", stop_err);
            if (stop_err == ESP_OK) {
                s_started = false;
                portENTER_CRITICAL(&s_lock);
                s_info.connected = false;
                portEXIT_CRITICAL(&s_lock);
                /* Stopping RF does not change APSTA configuration. Retain AP
                 * ownership until a checked mode change prevents its restart. */
                err = esp_wifi_set_mode(WIFI_MODE_STA);
                log_error("stopped AP mode reset", err);
            }
        }
        if (err == ESP_OK) s_ap_active = false;
        else status(COFFEE_NET_FAILED);
    }
    if (s_ap && !s_ap_active) { esp_netif_destroy_default_wifi(s_ap); s_ap = NULL; }
    s_ap_stop_at = 0;
    s_session_deadline = 0;
    s_cleanup_at = (s_http || s_ap_active) ? esp_timer_get_time() + 1000000 : 0;
}

static void cleanup_network(void)
{
    close_ap();
    if (s_sntp_initialized) { esp_netif_sntp_deinit(); s_sntp_initialized = false; }
    if (s_started) { log_error("Wi-Fi stop", esp_wifi_stop()); s_started = false; }
    if (s_wifi_registered) {
        log_error("Wi-Fi handler remove", esp_event_handler_instance_unregister(WIFI_EVENT,
            WIFI_EVENT_STA_DISCONNECTED, s_wifi_handler));
        s_wifi_registered = false;
    }
    if (s_ip_registered) {
        log_error("IP handler remove", esp_event_handler_instance_unregister(IP_EVENT,
            IP_EVENT_STA_GOT_IP, s_ip_handler));
        s_ip_registered = false;
    }
    if (s_wifi_initialized) { log_error("Wi-Fi deinit", esp_wifi_deinit()); s_wifi_initialized = false; }
    if (s_sta) { esp_netif_destroy_default_wifi(s_sta); s_sta = NULL; }
}

static esp_err_t prepare_network(void)
{
    if (s_wifi_initialized) return ESP_OK;
    esp_err_t err = nvs_flash_init(); /* Never erase the shared partition. */
    if (err != ESP_OK) return err;
    err = esp_netif_init();
    if (err != ESP_OK) return err;
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_WIFI_STA();
    s_sta = esp_netif_new(&cfg);
    if (!s_sta) return ESP_ERR_NO_MEM;
    err = esp_netif_attach_wifi_station(s_sta);
    if (err != ESP_OK) goto fail;
    err = esp_wifi_set_default_wifi_sta_handlers();
    if (err != ESP_OK) goto fail;
    wifi_init_config_t wifi = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&wifi);
    if (err != ESP_OK) goto fail;
    s_wifi_initialized = true;
    err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (err != ESP_OK) goto fail;
    err = esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED,
                                             event, NULL, &s_wifi_handler);
    if (err != ESP_OK) goto fail;
    s_wifi_registered = true;
    err = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                             event, NULL, &s_ip_handler);
    if (err != ESP_OK) goto fail;
    s_ip_registered = true;
    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) goto fail;
    return ESP_OK;
fail:
    cleanup_network();
    return err;
}

static esp_err_t load_credentials(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("coffee_wifi", NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (err != ESP_OK) return err;
    credential_blob_t blob = {0};
    size_t length = sizeof(blob);
    err = nvs_get_blob(handle, "credentials", &blob, &length);
    nvs_close(handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (err != ESP_OK || length != sizeof(blob) || blob.magic != CREDENTIAL_MAGIC ||
        blob.version != 1 || !coffee_credentials_valid(&blob.credentials)) {
        memset(&blob, 0, sizeof(blob));
        return err == ESP_OK ? ESP_ERR_INVALID_STATE : err;
    }
    s_saved = blob.credentials;
    s_have_saved = true;
    memset(&blob, 0, sizeof(blob));
    return ESP_OK;
}

static esp_err_t save_credentials(const coffee_credentials_t *credentials)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("coffee_wifi", NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;
    credential_blob_t blob = {.magic = CREDENTIAL_MAGIC, .version = 1,
                              .credentials = *credentials};
    err = nvs_set_blob(handle, "credentials", &blob, sizeof(blob));
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    memset(&blob, 0, sizeof(blob));
    return err;
}

static esp_err_t forget_credentials(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("coffee_wifi", NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;
    err = nvs_erase_key(handle, "credentials");
    if (err == ESP_ERR_NVS_NOT_FOUND) err = ESP_OK;
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}

static esp_err_t connect_station(const coffee_credentials_t *credentials)
{
    portENTER_CRITICAL(&s_lock);
    bool provisioning = s_info.provisioning;
    portEXIT_CRITICAL(&s_lock);
    if (s_ap_active && !provisioning) return ESP_ERR_INVALID_STATE;
    if (!s_ap_active) {
        /* A stopped radio can still remember APSTA. Never start a saved station
         * without first applying STA mode, including after cleanup failures. */
        esp_err_t mode_err = esp_wifi_set_mode(WIFI_MODE_STA);
        if (mode_err != ESP_OK) return mode_err;
    }
    wifi_config_t config = {0};
    memcpy(config.sta.ssid, credentials->ssid, strlen(credentials->ssid));
    memcpy(config.sta.password, credentials->password, strlen(credentials->password));
    config.sta.threshold.authmode = credentials->password[0] ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    config.sta.pmf_cfg.capable = true;
    config.sta.pmf_cfg.required = false;
    if (s_started) {
        esp_err_t err = esp_wifi_disconnect();
        if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_CONNECT) {
            memset(&config, 0, sizeof(config));
            return err;
        }
    }
    portENTER_CRITICAL(&s_lock);
    s_info.connected = false;
    portEXIT_CRITICAL(&s_lock);
    xEventGroupClearBits(s_link_events, LINK_UP | LINK_DOWN);
    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &config);
    memset(&config, 0, sizeof(config));
    if (err == ESP_OK && !s_started) {
        err = esp_wifi_start();
        if (err == ESP_OK) s_started = true;
    }
    if (err == ESP_OK) err = esp_wifi_connect();
    s_attempt = err == ESP_OK;
    s_attempt_deadline = esp_timer_get_time() + ATTEMPT_US;
    return err;
}

static void saved_connect(void)
{
    if (!s_have_saved) { s_retry_at = 0; return; }
    esp_err_t err = connect_station(&s_saved);
    log_error("saved network connect", err);
    if (err != ESP_OK) {
        s_attempt = false;
        s_retry_at = esp_timer_get_time() + 60000000LL;
        status(COFFEE_NET_FAILED);
    } else {
        s_retry_at = 0;
        if (!s_ap_active) status(COFFEE_NET_CONNECTING);
    }
}

static esp_err_t open_ap(void)
{
    close_ap();
    if (s_http || s_ap_active) return ESP_ERR_INVALID_STATE;
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_WIFI_AP();
    s_ap = esp_netif_new(&cfg);
    if (!s_ap) return ESP_ERR_NO_MEM;
    esp_err_t err = esp_netif_attach_wifi_ap(s_ap);
    if (err != ESP_OK) goto fail;
    err = esp_wifi_set_default_wifi_ap_handlers();
    if (err != ESP_OK) goto fail;
    /* Start STA radio first: hardware RNG requires active RF entropy. */
    if (!s_started) {
        err = esp_wifi_start();
        if (err != ESP_OK) goto fail;
        s_started = true;
    }
    wifi_config_t config = {0};
    snprintf((char *)config.ap.ssid, sizeof(config.ap.ssid), "CoffeeNotes-%04X",
             (unsigned)(esp_random() & 0xffff));
    /* Rejection sampling avoids modulo bias in the temporary eight-digit key. */
    for (unsigned i = 0; i < 8; ++i) {
        uint32_t random;
        do { random = esp_random(); } while (random >= UINT32_MAX - (UINT32_MAX % 10));
        config.ap.password[i] = (uint8_t)('0' + random % 10);
    }
    config.ap.ssid_len = strlen((char *)config.ap.ssid);
    config.ap.channel = 1;
    config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    config.ap.max_connection = 1;
    /* Configure while stopped so switching modes cannot briefly expose a
     * default/open AP. STA RF above supplied entropy for the key. */
    err = esp_wifi_stop();
    if (err != ESP_OK) goto fail_config;
    s_started = false;
    s_attempt = false;
    portENTER_CRITICAL(&s_lock);
    s_info.connected = false;
    portEXIT_CRITICAL(&s_lock);
    err = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (err != ESP_OK) goto fail_config;
    s_ap_active = true;
    err = esp_wifi_set_config(WIFI_IF_AP, &config);
    if (err != ESP_OK) goto fail_config;
    if (!s_started) {
        err = esp_wifi_start();
        if (err != ESP_OK) goto fail_config;
        s_started = true;
    }
    uint8_t random_token[16];
    esp_fill_random(random_token, sizeof(random_token));
    static const char hex[] = "0123456789abcdef";
    portENTER_CRITICAL(&s_lock);
    for (unsigned i = 0; i < sizeof(random_token); ++i) {
        s_token[i*2] = hex[random_token[i] >> 4];
        s_token[i*2+1] = hex[random_token[i] & 15];
    }
    s_token[COFFEE_FORM_TOKEN_LENGTH] = 0;
    memcpy(s_info.ap_ssid, config.ap.ssid, config.ap.ssid_len);
    s_info.ap_ssid[config.ap.ssid_len] = 0;
    memcpy(s_info.ap_password, config.ap.password, 8);
    s_info.ap_password[8] = 0;
    s_info.provisioning = true;
    portEXIT_CRITICAL(&s_lock);
    memset(random_token, 0, sizeof(random_token));
    memset(&config, 0, sizeof(config));
    httpd_config_t http = HTTPD_DEFAULT_CONFIG();
    http.max_open_sockets = 3;
    http.backlog_conn = 2;
    http.max_uri_handlers = 3;
    http.stack_size = 4096;
    http.recv_wait_timeout = 3;
    http.send_wait_timeout = 3;
    http.lru_purge_enable = true;
    err = httpd_start(&s_http, &http);
    if (err != ESP_OK) goto fail;
    const httpd_uri_t routes[] = {
        {.uri = "/", .method = HTTP_GET, .handler = page_get},
        {.uri = "/status", .method = HTTP_GET, .handler = status_get},
        {.uri = "/configure", .method = HTTP_POST, .handler = configure_post},
    };
    for (unsigned i = 0; i < sizeof(routes) / sizeof(routes[0]); ++i) {
        err = httpd_register_uri_handler(s_http, &routes[i]);
        if (err != ESP_OK) goto fail;
    }
    s_session_deadline = esp_timer_get_time() + SESSION_US;
    if (s_have_saved) saved_connect();
    status(COFFEE_NET_AP_READY);
    return ESP_OK;
fail_config:
    memset(&config, 0, sizeof(config));
fail:
    close_ap();
    return err;
}

static void cancel_session(void)
{
    bool was_testing = s_testing;
    close_ap();
    s_testing = false;
    memset(&s_candidate, 0, sizeof(s_candidate));
    if (was_testing) {
        s_attempt = false;
        if (s_started) log_error("candidate disconnect", esp_wifi_disconnect());
        if (s_have_saved) saved_connect();
    } else if (!s_started && s_have_saved && s_wifi_initialized && !s_ap_active) {
        saved_connect();
    }
    coffee_network_info_t info;
    coffee_network_snapshot(&info);
    status((s_http || s_ap_active) ? COFFEE_NET_FAILED :
           info.connected ? (info.clock_valid ? COFFEE_NET_SYNCED : COFFEE_NET_ONLINE) :
           (s_have_saved ? COFFEE_NET_CONNECTING : COFFEE_NET_NEED_SETUP));
}

static void candidate_failed(void)
{
    s_testing = false;
    s_attempt = false;
    memset(&s_candidate, 0, sizeof(s_candidate));
    portENTER_CRITICAL(&s_lock);
    s_candidate_pending = false;
    portEXIT_CRITICAL(&s_lock);
    if (s_started) log_error("failed candidate disconnect", esp_wifi_disconnect());
    if (s_have_saved) saved_connect();
    status(COFFEE_NET_FAILED);
}

static void time_synced(struct timeval *value)
{
    /* Runs on the TCP/IP task immediately after the genuine clock update.
     * Publish its provenance here; a delayed worker must not make an older
     * sync appear newer than a manual clock change. */
    if (!value) return;
    uint64_t synchronized_us = (uint64_t)esp_timer_get_time();
    bool valid = (int64_t)value->tv_sec >= 1577836800LL &&
                 (int64_t)value->tv_sec < 4102444800LL;
    portENTER_CRITICAL(&s_lock);
    s_info.clock_valid = valid;
    if (valid) {
        ++s_info.sync_generation;
        s_info.sync_monotonic_us = synchronized_us;
    }
    portEXIT_CRITICAL(&s_lock);
}

static void start_sntp(void)
{
    esp_err_t err;
    if (!s_sntp_initialized) {
        esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(2,
            ESP_SNTP_SERVER_LIST("ntp.aliyun.com", "pool.ntp.org"));
        config.sync_cb = time_synced;
        err = esp_netif_sntp_init(&config);
        if (err == ESP_OK) s_sntp_initialized = true;
    } else err = esp_netif_sntp_start();
    log_error("SNTP start", err);
}

static bool verify_link(void)
{
    wifi_ap_record_t ap;
    esp_netif_ip_info_t ip;
    const coffee_credentials_t *expected = s_testing ? &s_candidate : &s_saved;
    if ((!s_testing && !s_have_saved) || esp_wifi_sta_get_ap_info(&ap) != ESP_OK ||
        esp_netif_get_ip_info(s_sta, &ip) != ESP_OK || !ip.ip.addr) return false;
    size_t length = strlen(expected->ssid);
    size_t actual = strnlen((const char *)ap.ssid, sizeof(ap.ssid));
    return length == actual && !memcmp(ap.ssid, expected->ssid, length);
}

static void got_ip(void)
{
    if (!verify_link()) return;
    portENTER_CRITICAL(&s_lock);
    s_info.connected = true;
    portEXIT_CRITICAL(&s_lock);
    s_attempt = false;
    s_retry_at = 0;
    s_retries = 0;
    if (s_testing) {
        esp_err_t err = save_credentials(&s_candidate);
        log_error("credential save", err);
        if (err == ESP_OK) { s_saved = s_candidate; s_have_saved = true; }
        s_testing = false;
        memset(&s_candidate, 0, sizeof(s_candidate));
        status(err == ESP_OK ? COFFEE_NET_SAVED : COFFEE_NET_STORE_FAILED);
        /* Allow the phone to observe verified persistence, then shut the AP down. */
        s_ap_stop_at = esp_timer_get_time() + 5000000;
    } else if (!s_ap_active) status(COFFEE_NET_ONLINE);
    start_sntp();
}

static void handle_command(command_t *cmd)
{
    switch (cmd->kind) {
    case CMD_SETUP: {
        if (s_ap_active) break; /* Keep current session and token stable. */
        esp_err_t err = prepare_network();
        if (err == ESP_OK) err = open_ap();
        log_error("provisioning start", err);
        if (err != ESP_OK) {
            if (s_wifi_initialized && s_have_saved && !s_ap_active) saved_connect();
            status(COFFEE_NET_FAILED);
        }
        break;
    }
    case CMD_CANCEL:
        cancel_session();
        break;
    case CMD_FORGET: {
        close_ap();
        s_testing = false;
        s_attempt = false;
        s_retry_at = 0;
        memset(&s_candidate, 0, sizeof(s_candidate));
        esp_err_t err = forget_credentials();
        log_error("credential clear", err);
        if (err == ESP_OK) {
            s_have_saved = false;
            memset(&s_saved, 0, sizeof(s_saved));
            if (s_started) log_error("forgotten network disconnect", esp_wifi_disconnect());
            portENTER_CRITICAL(&s_lock);
            s_info.connected = false;
            portEXIT_CRITICAL(&s_lock);
            status(COFFEE_NET_NEED_SETUP);
        } else {
            if (s_have_saved) saved_connect();
            status(COFFEE_NET_STORE_FAILED);
        }
        break;
    }
    case CMD_CANDIDATE: {
        portENTER_CRITICAL(&s_lock);
        bool valid = s_info.provisioning && !strcmp(cmd->token, s_token);
        portEXIT_CRITICAL(&s_lock);
        if (!valid || s_testing || s_ap_stop_at) break;
        s_candidate = cmd->credentials;
        s_testing = true;
        s_retry_at = 0;
        status(COFFEE_NET_TESTING);
        esp_err_t err = connect_station(&s_candidate);
        log_error("candidate connect", err);
        if (err != ESP_OK) candidate_failed();
        break;
    }
    }
}

static void worker(void *arg)
{
    (void)arg;
    if (setenv("TZ", "CST-8", 1) != 0) {
        status(COFFEE_NET_FAILED);
        ESP_LOGE(TAG, "Timezone configuration failed");
    } else tzset();
    esp_err_t err = prepare_network();
    if (err == ESP_OK) err = load_credentials();
    log_error("network initialization", err);
    if (err != ESP_OK) status(COFFEE_NET_FAILED);
    else if (s_have_saved) saved_connect();
    else status(COFFEE_NET_NEED_SETUP);
    for (;;) {
        command_t cmd;
        if (xQueueReceive(s_commands, &cmd, pdMS_TO_TICKS(200)) == pdTRUE) {
            handle_command(&cmd);
            memset(&cmd, 0, sizeof(cmd));
        }
        int64_t now = esp_timer_get_time();
        EventBits_t bits = xEventGroupWaitBits(s_link_events, LINK_UP | LINK_DOWN,
                                             pdTRUE, pdFALSE, 0);
        if (bits & LINK_DOWN) {
            portENTER_CRITICAL(&s_lock);
            s_info.connected = false;
            portEXIT_CRITICAL(&s_lock);
            if (!s_testing && s_have_saved && !s_attempt && !s_retry_at) {
                s_retry_at = now + 5000000;
                if (!s_ap_active) status(COFFEE_NET_CONNECTING);
            }
        }
        if (bits & LINK_UP) got_ip();
        if (s_sntp_initialized && esp_netif_sntp_sync_wait(0) == ESP_OK) {
            /* Callback already published time provenance atomically. */
            portENTER_CRITICAL(&s_lock);
            if (!s_info.provisioning && s_info.status != COFFEE_NET_SAVED &&
                s_info.status != COFFEE_NET_STORE_FAILED && s_info.connected)
                s_info.status = s_info.clock_valid ? COFFEE_NET_SYNCED : COFFEE_NET_ONLINE;
            portEXIT_CRITICAL(&s_lock);
        }
        if (s_cleanup_at && now >= s_cleanup_at) close_ap();
        if (s_ap_active && s_session_deadline && now >= s_session_deadline) cancel_session();
        else if (s_ap_stop_at && now >= s_ap_stop_at) close_ap();
        if (s_attempt && now >= s_attempt_deadline) {
            if (s_testing) candidate_failed();
            else {
                s_attempt = false;
                if (s_started) log_error("timed out station disconnect", esp_wifi_disconnect());
                ++s_retries;
                /* Three attempts per burst, then a bounded five-minute offline backoff. */
                s_retry_at = now + (s_retries >= 3 ? 300000000LL : (5000000LL << s_retries));
                if (s_retries >= 3) s_retries = 0;
                if (!s_ap_active) status(COFFEE_NET_FAILED);
            }
        }
        if (!s_testing && s_have_saved && !s_attempt && s_retry_at && now >= s_retry_at)
            saved_connect();
    }
}

bool coffee_network_init(void)
{
    portENTER_CRITICAL(&s_lock);
    bool claimed = s_init_claimed;
    s_init_claimed = true;
    portEXIT_CRITICAL(&s_lock);
    if (claimed) return s_commands != NULL;
    s_commands = xQueueCreate(4, sizeof(command_t));
    s_link_events = xEventGroupCreate();
    if (!s_commands || !s_link_events ||
        xTaskCreate(worker, "coffee_network", 5120, NULL, 4, NULL) != pdPASS) {
        if (s_commands) { vQueueDelete(s_commands); s_commands = NULL; }
        if (s_link_events) { vEventGroupDelete(s_link_events); s_link_events = NULL; }
        status(COFFEE_NET_FAILED);
        portENTER_CRITICAL(&s_lock);
        s_init_claimed = false;
        portEXIT_CRITICAL(&s_lock);
        return false;
    }
    return true;
}
