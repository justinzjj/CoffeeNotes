#include "coffee_store.h"
#include "nvs_flash.h"

/* Shared only by the single owning application task; no large stack buffers. */
static uint8_t wire[COFFEE_WIRE_MAX];

static esp_err_t stop_store(coffee_store_t *store, esp_err_t error)
{
    if (store->handle) {
        nvs_close(store->handle);
        store->handle = 0;
    }
    store->ready = false;
    return error;
}

esp_err_t coffee_store_init(coffee_store_t *store, coffee_data_t *data)
{
    if (!store || !data) return ESP_ERR_INVALID_ARG;
    *store = (coffee_store_t){0};
    esp_err_t error = nvs_flash_init();
    if (error != ESP_OK) {
        store->read_failed = true;
        return error;
    }
    error = nvs_open("coffee_notes", NVS_READWRITE, &store->handle);
    if (error != ESP_OK) {
        store->read_failed = true;
        return stop_store(store, error);
    }
    size_t length = 0;
    error = nvs_get_blob(store->handle, "journal", NULL, &length);
    if (error == ESP_ERR_NVS_NOT_FOUND) {
        store->ready = true;
        return ESP_OK;
    }
    if (error != ESP_OK) {
        store->read_failed = true;
        return stop_store(store, error);
    }
    if (length < 48 || length > sizeof(wire)) {
        store->corrupt = true;
        return stop_store(store, ESP_ERR_INVALID_SIZE);
    }
    size_t read_length = length;
    error = nvs_get_blob(store->handle, "journal", wire, &read_length);
    if (error != ESP_OK) {
        store->read_failed = true;
        return stop_store(store, error);
    }
    if (read_length != length) {
        store->corrupt = true;
        return stop_store(store, ESP_ERR_INVALID_SIZE);
    }
    /* Decoder validates every field and CRC before touching data. */
    if (!coffee_decode(data, wire, length)) {
        store->corrupt = true;
        return stop_store(store, ESP_ERR_INVALID_RESPONSE);
    }
    store->ready = true;
    return ESP_OK;
}

esp_err_t coffee_store_save(coffee_store_t *store, const coffee_data_t *data)
{
    if (!store || !data) return ESP_ERR_INVALID_ARG;
    if (!store->ready) return ESP_ERR_INVALID_STATE;
    size_t length = coffee_encode(data, wire, sizeof(wire));
    if (!length) return ESP_ERR_INVALID_ARG;
    esp_err_t error = nvs_set_blob(store->handle, "journal", wire, length);
    if (error != ESP_OK) return stop_store(store, error);
    error = nvs_commit(store->handle);
    /* Both setter and commit failures can leave bytes written. Do not retry,
     * erase, or publish the candidate as successful; reload at next startup. */
    if (error != ESP_OK) return stop_store(store, error);
    return ESP_OK;
}
