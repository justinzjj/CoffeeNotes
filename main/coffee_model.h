#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define COFFEE_CAPACITY 512
#define COFFEE_RECIPE_COUNT 3
#define COFFEE_TYPE_COUNT 6
#define COFFEE_THEME_COUNT 4
#define COFFEE_NO_RECIPE 255
#define COFFEE_WIRE_MAX (48 + COFFEE_CAPACITY * 12)

typedef enum { COFFEE_TODAY, COFFEE_WEEK, COFFEE_MONTH, COFFEE_PERIOD_COUNT } coffee_period_t;

typedef enum { COFFEE_HAND, COFFEE_ESPRESSO, COFFEE_AMERICANO, COFFEE_LATTE, COFFEE_COLD, COFFEE_OTHER } coffee_type_t;
typedef struct { uint16_t grams, water_ml, degrees, seconds; } coffee_recipe_t;
typedef struct {
    uint32_t date;
    uint16_t minute, duration;
    uint8_t type, recipe, manual_time;
} coffee_entry_t;
typedef struct {
    uint16_t count;
    uint32_t last_date;
    uint16_t last_minute;
    uint8_t home_period, theme;
    coffee_recipe_t recipes[COFFEE_RECIPE_COUNT];
    coffee_entry_t entries[COFFEE_CAPACITY];
} coffee_data_t;
typedef enum {
    COFFEE_HOME, COFFEE_CALENDAR, COFFEE_DAY, COFFEE_RECORD,
    COFFEE_RECIPES, COFFEE_RECIPE, COFFEE_EDIT_RECIPE, COFFEE_TIMER,
    COFFEE_SETTINGS, COFFEE_THEMES, COFFEE_DATE, COFFEE_NETWORK, COFFEE_DELETE, COFFEE_FORGET
} coffee_page_t;
typedef enum { COFFEE_UP, COFFEE_DOWN, COFFEE_OK, COFFEE_BACK, COFFEE_PREV_MONTH, COFFEE_NEXT_MONTH } coffee_key_t;
typedef enum {
    COFFEE_ACTION_NONE, COFFEE_ACTION_RECORD, COFFEE_ACTION_DELETE,
    COFFEE_ACTION_RECIPE, COFFEE_ACTION_CLOCK, COFFEE_ACTION_PREFERENCES, COFFEE_ACTION_SETUP,
    COFFEE_ACTION_CANCEL_SETUP, COFFEE_ACTION_FORGET
} coffee_action_t;
typedef enum { COFFEE_NOTICE_NONE, COFFEE_NOTICE_SAVED, COFFEE_NOTICE_FAILED, COFFEE_NOTICE_FULL, COFFEE_NOTICE_BAD_DATE } coffee_notice_t;
typedef struct {
    coffee_data_t *data;
    coffee_page_t page, date_return;
    uint16_t focus, day_focus;
    uint8_t recipe, type, date_field, draft_period, draft_theme;
    uint32_t cursor, now_date, draft_date, date_backup;
    uint16_t now_minute, draft_minute, record_seconds, minute_backup;
    bool clock_valid, clock_manual, draft_clock_valid, manually_set, editing, date_for_clock;
    coffee_recipe_t draft_recipe;
    bool timer_active, timer_paused;
    uint64_t timer_anchor, timer_accum;
    coffee_notice_t notice;
} coffee_model_t;

bool coffee_date_valid(uint32_t date);
unsigned coffee_month_days(unsigned year, unsigned month);
uint32_t coffee_date_step(uint32_t date, int delta);
uint32_t coffee_date_month(uint32_t date, int delta);
unsigned coffee_weekday(uint32_t date); /* Monday = 0 */
/* Natural-week bounds are clipped to the supported 2020–2099 date range. Invalid dates return zero. */
uint32_t coffee_week_start(uint32_t date);
uint32_t coffee_week_end(uint32_t date);
unsigned coffee_week_count(const coffee_data_t *data, uint32_t date);
unsigned coffee_period_count(const coffee_data_t *data, uint32_t date, coffee_period_t period);
unsigned coffee_day_count(const coffee_data_t *data, uint32_t date);
int coffee_day_entry(const coffee_data_t *data, uint32_t date, unsigned index);
unsigned coffee_month_count(const coffee_data_t *data, uint32_t date);
bool coffee_recipe_valid(const coffee_recipe_t *recipe);
void coffee_data_init(coffee_data_t *data, uint32_t seed_date);
void coffee_model_init(coffee_model_t *model, coffee_data_t *data);
void coffee_model_clock(coffee_model_t *model, uint32_t date, unsigned minute, bool synced);
coffee_action_t coffee_model_key(coffee_model_t *model, coffee_key_t key, uint64_t now_ms);
uint32_t coffee_timer_seconds(const coffee_model_t *model, uint64_t now_ms);
unsigned coffee_timer_stage(const coffee_model_t *model, uint64_t now_ms);
unsigned coffee_stage_water(const coffee_recipe_t *recipe, unsigned stage);
bool coffee_prepare_change(const coffee_model_t *model, coffee_action_t action, coffee_data_t *candidate);
void coffee_model_complete(coffee_model_t *model, coffee_action_t action, bool success);
size_t coffee_encode(const coffee_data_t *data, uint8_t *out, size_t capacity);
bool coffee_decode(coffee_data_t *data, const uint8_t *input, size_t length);
