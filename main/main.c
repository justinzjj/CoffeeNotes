#include "coffee_model.h"
#include "coffee_store.h"
#include "coffee_network.h"
#include "coffee_ui.h"
#include "bsp_display.h"
#include "bsp_button.h"
#include "bsp_battery.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

static const char *TAG = "coffee";
static coffee_data_t data, candidate;
static coffee_model_t model;
static coffee_store_t store;
static coffee_ui_info_t ui;
static QueueHandle_t keys;
static atomic_bool ready;
static uint64_t manual_apply_us;
typedef struct { bsp_btn_t button; bsp_btn_ev_t event; } key_event_t;

static void on_key(bsp_btn_t button, bsp_btn_ev_t event, void *arg)
{
    (void)arg;
    if (!atomic_load(&ready)) return;
    const key_event_t key = {button,event};
    (void)xQueueSend(keys, &key, 0);
}
static bool translate(const key_event_t *event, coffee_key_t *key)
{
    if (event->event == BSP_BTN_LONG) {
        *key = event->button == BSP_BTN_OK ? COFFEE_BACK : event->button == BSP_BTN_UP ? COFFEE_PREV_MONTH : COFFEE_NEXT_MONTH;
        return true;
    }
    if (event->event != BSP_BTN_CLICK && event->event != BSP_BTN_DOUBLE) return false;
    *key = event->button == BSP_BTN_UP ? COFFEE_UP : event->button == BSP_BTN_DOWN ? COFFEE_DOWN : COFFEE_OK;
    return true;
}
static bool set_manual_clock(void)
{
    struct tm local = {0};
    local.tm_year = (int)(model.draft_date / 10000) - 1900;
    local.tm_mon = (int)(model.draft_date / 100 % 100) - 1;
    local.tm_mday = (int)(model.draft_date % 100);
    local.tm_hour = model.draft_minute / 60; local.tm_min = model.draft_minute % 60;
    local.tm_isdst = -1;
    time_t epoch = mktime(&local);
    if (epoch == (time_t)-1) return false;
    const struct timeval tv = {.tv_sec = epoch};
    if (settimeofday(&tv, NULL) != 0) return false;
    /* Conservatively retain manual provenance if sync happens during this
     * small measurement window; a later genuine sync clears it. */
    manual_apply_us = (uint64_t)esp_timer_get_time();
    return true;
}
static bool perform(coffee_action_t action)
{
    if (action == COFFEE_ACTION_NONE) return true;
    if (action == COFFEE_ACTION_SETUP) return coffee_network_setup();
    if (action == COFFEE_ACTION_CANCEL_SETUP) return coffee_network_cancel();
    if (action == COFFEE_ACTION_FORGET) return coffee_network_forget();
    bool success = coffee_prepare_change(&model, action, &candidate);
    esp_err_t error = success ? coffee_store_save(&store, &candidate) : ESP_ERR_INVALID_ARG;
    success = error == ESP_OK;
    if (success) {
        data = candidate;
        if (action == COFFEE_ACTION_CLOCK) success = set_manual_clock();
    }
    coffee_model_complete(&model, action, success);
    ui.storage_ready = store.ready;
    if (!success) ESP_LOGE(TAG, "Action %u not confirmed: %s", (unsigned)action, esp_err_to_name(error));
    return success;
}
static void worker(void *arg)
{
    (void)arg;
    bool dirty = true, dimmed = false;
    int wake_button = -1;
    uint64_t last_input = 0, last_battery = 0, notice_until = 0;
    uint32_t last_second = UINT32_MAX, sync_generation = 0;
    for (;;) {
        key_event_t event;
        bool received = xQueueReceive(keys, &event, pdMS_TO_TICKS(100)) == pdTRUE;
        uint64_t now = (uint64_t)esp_timer_get_time() / 1000;
        if (received) {
            last_input = now;
            if (dimmed) {
                bsp_display_backlight(85); dimmed = false;
                if (event.event == BSP_BTN_PRESS) wake_button = (int)event.button;
                /* Consume the complete first gesture after idle dimming. */
                received = false;
            } else if (wake_button == (int)event.button) {
                if (event.event != BSP_BTN_PRESS) wake_button = -1;
                received = false;
            }
        }
        coffee_network_info_t net = {0}; coffee_network_snapshot(&net);
        if (memcmp(&net, &ui.network, sizeof(net))) { ui.network = net; dirty = true; }
        if (net.sync_generation != sync_generation) {
            sync_generation = net.sync_generation;
            if (net.sync_monotonic_us > manual_apply_us) model.clock_manual = false;
        }
        if (net.clock_valid || model.clock_manual) {
            time_t epoch = time(NULL); struct tm local;
            if (localtime_r(&epoch, &local)) {
                uint32_t date = (uint32_t)(local.tm_year + 1900) * 10000 + (uint32_t)(local.tm_mon + 1) * 100 + (uint32_t)local.tm_mday;
                unsigned minute = (unsigned)local.tm_hour * 60 + (unsigned)local.tm_min;
                if (date != model.now_date || minute != model.now_minute || !model.clock_valid) dirty = true;
                coffee_model_clock(&model, date, minute, net.clock_valid && !model.clock_manual);
            }
        }
        if (received) {
            coffee_key_t key;
            if (translate(&event, &key)) {
                coffee_action_t action = coffee_model_key(&model, key, now);
                if (!perform(action) && action >= COFFEE_ACTION_SETUP) model.notice = COFFEE_NOTICE_FAILED;
                dirty = true;
                if (model.notice != COFFEE_NOTICE_NONE) notice_until = now + 2500;
            }
        }
        if (notice_until && now >= notice_until) { model.notice = COFFEE_NOTICE_NONE; notice_until = 0; dirty = true; }
        if (now - last_battery >= 10000 || last_battery == 0) {
            int battery = bsp_battery_soc();
            if (battery != ui.battery) { ui.battery = battery; dirty = true; }
            last_battery = now;
        }
        if (model.page == COFFEE_TIMER) {
            uint32_t second = coffee_timer_seconds(&model, now);
            if (second != last_second) { last_second = second; dirty = true; }
        } else last_second = UINT32_MAX;
        if (!dimmed && now - last_input >= 60000 && !model.timer_active && !net.provisioning) {
            bsp_display_backlight(15); dimmed = true;
        }
        if (dirty && bsp_lvgl_lock(500)) {
            coffee_ui_render(&model, &ui, now);
            bsp_lvgl_unlock(); dirty = false;
        }
    }
}
void app_main(void)
{
    ESP_LOGI(TAG, "CoffeeNotes 1.0.0 starting");
    setenv("TZ", "CST-8", 1); tzset();
    /* A build-date draft is never accepted as a real clock on cold boot. */
    coffee_data_init(&data, 20261006);
    esp_err_t storage_error = coffee_store_init(&store, &data);
    if (storage_error != ESP_OK) ESP_LOGE(TAG, "Storage read-only: %s", esp_err_to_name(storage_error));
    coffee_model_init(&model, &data);
    if (bsp_display_init() != ESP_OK || !bsp_lvgl_init()) { ESP_LOGE(TAG, "Display initialization failed"); return; }
    bsp_display_backlight(85);
    ui.battery = -1; ui.storage_ready = store.ready;
    if (!bsp_lvgl_lock(1000)) { ESP_LOGE(TAG, "LVGL lock unavailable"); return; }
    bool fonts_ok = coffee_ui_verify_fonts();
    bool screen_ok = fonts_ok && coffee_ui_create();
    if (screen_ok) coffee_ui_render(&model, &ui, 0);
    bsp_lvgl_unlock();
    if (!screen_ok) { ESP_LOGE(TAG, "UI/font validation failed"); return; }
    if (bsp_battery_init() != ESP_OK) ESP_LOGW(TAG, "Battery gauge unavailable");
    keys = xQueueCreate(16, sizeof(key_event_t));
    if (!keys) { ESP_LOGE(TAG, "Input queue allocation failed"); return; }
    ui.input_ready = bsp_button_init(on_key, NULL) == ESP_OK;
    if (!coffee_network_init()) ESP_LOGW(TAG, "Network service unavailable; manual date remains usable");
    if (xTaskCreate(worker, "coffee_app", 8192, NULL, 4, NULL) != pdPASS) {
        ui.input_ready = false;
        if (bsp_lvgl_lock(500)) { coffee_ui_render(&model, &ui, 0); bsp_lvgl_unlock(); }
        ESP_LOGE(TAG, "Application worker allocation failed"); return;
    }
    atomic_store(&ready, ui.input_ready);
    ESP_LOGI(TAG, "Ready: entries=%u, storage=%d, input=%d, heap=%lu, largest=%lu",
             data.count, store.ready, ui.input_ready, (unsigned long)esp_get_free_heap_size(),
             (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
}
