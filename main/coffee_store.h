#pragma once
#include <stdbool.h>
#include "esp_err.h"
#include "nvs.h"
#include "coffee_model.h"

typedef struct {
    nvs_handle_t handle;
    bool ready;
    bool corrupt;
    bool read_failed;
} coffee_store_t;

/* Blocking calls owned by the single application task. Initialize once at
 * startup with data already initialized; never erase unreadable storage.
 * A failed write disables further saves until the next startup. */
esp_err_t coffee_store_init(coffee_store_t *store, coffee_data_t *data);
esp_err_t coffee_store_save(coffee_store_t *store, const coffee_data_t *data);
