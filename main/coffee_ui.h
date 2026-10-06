#pragma once
#include "coffee_model.h"
#include "coffee_network.h"
#include <stdbool.h>

typedef struct {
    int battery;
    bool storage_ready, input_ready;
    coffee_network_info_t network;
} coffee_ui_info_t;
/* Caller holds BSP LVGL lock (or is the LVGL host-preview thread). */
bool coffee_ui_create(void);
void coffee_ui_render(const coffee_model_t *model, const coffee_ui_info_t *info, uint64_t now_ms);
bool coffee_ui_verify_fonts(void);
