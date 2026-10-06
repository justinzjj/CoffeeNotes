#include "coffee_model.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static coffee_data_t d, candidate, loaded;
static uint8_t wire[COFFEE_WIRE_MAX];

static void update_crc(uint8_t *bytes, size_t length)
{
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0; i < length; ++i) {
        if (i >= 44 && i < 48) continue;
        crc ^= bytes[i];
        for (unsigned bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U)));
    }
    crc = ~crc;
    for (unsigned i = 0; i < 4; ++i) bytes[44 + i] = (uint8_t)(crc >> (8 * i));
}
static void natural_week_statistics(void)
{
    const struct { uint32_t date, start, end; } weeks[] = {
        {20261005,20261005,20261011}, {20261011,20261005,20261011},
        {20261101,20261026,20261101}, {20260101,20251229,20260104},
        {20240229,20240226,20240303}, {20200101,20200101,20200105},
        {20991231,20991228,20991231}
    };
    for (unsigned i = 0; i < sizeof(weeks) / sizeof(weeks[0]); ++i) {
        assert(coffee_week_start(weeks[i].date) == weeks[i].start);
        assert(coffee_week_end(weeks[i].date) == weeks[i].end);
        coffee_data_init(&d, weeks[i].date);
        const uint32_t dates[] = {weeks[i].start,weeks[i].date,weeks[i].end};
        for (unsigned j = 0; j < 3; ++j) d.entries[d.count++] = (coffee_entry_t){dates[j],600,0,COFFEE_HAND,COFFEE_NO_RECIPE,0};
        assert(coffee_week_count(&d, weeks[i].date) == 3);
        if (weeks[i].start > 20200101) {
            d.entries[d.count++] = (coffee_entry_t){coffee_date_step(weeks[i].start,-1),600,0,COFFEE_HAND,COFFEE_NO_RECIPE,0};
        }
        if (weeks[i].end < 20991231) {
            d.entries[d.count++] = (coffee_entry_t){coffee_date_step(weeks[i].end,1),600,0,COFFEE_HAND,COFFEE_NO_RECIPE,0};
        }
        assert(coffee_period_count(&d, weeks[i].date, COFFEE_WEEK) == 3);
        assert(coffee_period_count(&d, weeks[i].date, COFFEE_TODAY) == coffee_day_count(&d, weeks[i].date));
        assert(coffee_period_count(&d, weeks[i].date, COFFEE_MONTH) == coffee_month_count(&d, weeks[i].date));
    }
    assert(coffee_week_start(20230229) == 0 && coffee_week_end(20230229) == 0);
    assert(coffee_week_count(&d, 20230229) == 0);
    assert(coffee_period_count(&d, 20230229, COFFEE_TODAY) == 0);
    assert(coffee_period_count(&d, 20230229, COFFEE_MONTH) == 0);
    assert(coffee_period_count(&d, 20261006, COFFEE_PERIOD_COUNT) == 0);
    assert(coffee_period_count(&d, 20261006, (coffee_period_t)-1) == 0);
}
static void preferences_are_transactional(void)
{
    coffee_data_init(&d, 20261006);
    d.count = 1; d.entries[0] = (coffee_entry_t){20261006,600,0,COFFEE_LATTE,COFFEE_NO_RECIPE,0};
    coffee_model_t m; coffee_model_init(&m, &d);
    assert(m.focus == 0 && d.home_period == COFFEE_TODAY && d.theme == 0);
    coffee_model_key(&m, COFFEE_UP, 0);
    assert(m.focus == 4);
    for (unsigned period = 0; period < COFFEE_PERIOD_COUNT; ++period) {
        unsigned next = (period + 1) % COFFEE_PERIOD_COUNT;
        assert(coffee_model_key(&m, COFFEE_OK, 0) == COFFEE_ACTION_PREFERENCES);
        assert(m.draft_period == next && m.draft_theme == 0 && d.home_period == period);
        assert(coffee_prepare_change(&m, COFFEE_ACTION_PREFERENCES, &candidate));
        assert(candidate.home_period == next && candidate.count == 1);
        coffee_model_complete(&m, COFFEE_ACTION_PREFERENCES, false);
        assert(m.page == COFFEE_HOME && m.focus == 4 && d.home_period == period && m.notice == COFFEE_NOTICE_FAILED);
        d = candidate; coffee_model_complete(&m, COFFEE_ACTION_PREFERENCES, true);
        assert(m.page == COFFEE_HOME && m.focus == 4 && d.home_period == next);
    }
    m.focus = 3; coffee_model_key(&m, COFFEE_OK, 0);
    assert(m.page == COFFEE_SETTINGS && m.focus == 0);
    coffee_model_key(&m, COFFEE_UP, 0); assert(m.focus == 3);
    coffee_model_key(&m, COFFEE_UP, 0); assert(m.focus == 2);
    coffee_model_key(&m, COFFEE_OK, 0);
    assert(m.page == COFFEE_THEMES && m.draft_theme == d.theme && m.draft_period == d.home_period);
    coffee_model_key(&m, COFFEE_UP, 0); assert(m.draft_theme == COFFEE_THEME_COUNT - 1 && d.theme == 0);
    coffee_model_key(&m, COFFEE_DOWN, 0); assert(m.draft_theme == 0);
    coffee_model_key(&m, COFFEE_DOWN, 0); assert(m.draft_theme == 1);
    coffee_model_key(&m, COFFEE_BACK, 0);
    assert(m.page == COFFEE_SETTINGS && m.focus == 2 && d.theme == 0);
    coffee_model_key(&m, COFFEE_OK, 0); assert(m.draft_theme == 0);
    for (unsigned theme = 1; theme < COFFEE_THEME_COUNT; ++theme) {
        coffee_model_key(&m, COFFEE_DOWN, 0);
        assert(coffee_model_key(&m, COFFEE_OK, 0) == COFFEE_ACTION_PREFERENCES);
        assert(coffee_prepare_change(&m, COFFEE_ACTION_PREFERENCES, &candidate));
        assert(candidate.theme == theme && candidate.home_period == d.home_period);
        assert(candidate.count == d.count && memcmp(candidate.entries, d.entries, sizeof(d.entries)) == 0);
        coffee_model_complete(&m, COFFEE_ACTION_PREFERENCES, false);
        assert(m.page == COFFEE_THEMES && d.theme == theme - 1 && m.notice == COFFEE_NOTICE_FAILED);
        d = candidate; coffee_model_complete(&m, COFFEE_ACTION_PREFERENCES, true);
        assert(m.page == COFFEE_SETTINGS && m.focus == 2 && d.theme == theme);
        coffee_model_key(&m, COFFEE_OK, 0);
    }
    m.draft_theme = COFFEE_THEME_COUNT;
    assert(!coffee_prepare_change(&m, COFFEE_ACTION_PREFERENCES, &candidate));
    m.draft_theme = 0; m.draft_period = COFFEE_PERIOD_COUNT;
    assert(!coffee_prepare_change(&m, COFFEE_ACTION_PREFERENCES, &candidate));
    coffee_model_key(&m, COFFEE_BACK, 0);
    coffee_model_key(&m, COFFEE_DOWN, 0); assert(m.focus == 3);
    coffee_model_key(&m, COFFEE_OK, 0); assert(m.page == COFFEE_FORGET);
    coffee_model_key(&m, COFFEE_BACK, 0); assert(m.page == COFFEE_SETTINGS && m.focus == 3);
}
static void preferences_codec(void)
{
    /* Immutable CFN1/version-1 fixture created before preferences used reserved bytes. */
    static const uint8_t old_blob[] = {0x43, 0x46, 0x4e, 0x31, 0x01, 0x00, 0x01, 0x00, 0x8e, 0x28, 0x35, 0x01, 0x1c, 0x02, 0x00, 0x00, 0x0f, 0x00, 0xf0, 0x00, 0x5c, 0x00, 0x96, 0x00, 0x14, 0x00, 0x2c, 0x01, 0x5e, 0x00, 0xb4, 0x00, 0x0f, 0x00, 0x96, 0x00, 0x5b, 0x00, 0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5d, 0x54, 0x9e, 0x98, 0x8e, 0x28, 0x35, 0x01, 0x58, 0x02, 0x00, 0x00, 0x03, 0xff, 0x01, 0x00};
    assert(coffee_decode(&loaded, old_blob, sizeof(old_blob)));
    assert(loaded.home_period == COFFEE_TODAY && loaded.theme == 0 && loaded.count == 1);
    assert(loaded.entries[0].date == 20261006 && loaded.entries[0].type == COFFEE_LATTE && loaded.entries[0].minute == 600);
    d = loaded;
    for (unsigned period = 0; period < COFFEE_PERIOD_COUNT; ++period) {
        for (unsigned theme = 0; theme < COFFEE_THEME_COUNT; ++theme) {
            d.home_period = (uint8_t)period; d.theme = (uint8_t)theme;
            size_t n = coffee_encode(&d, wire, sizeof(wire));
            assert(n == sizeof(old_blob) && wire[4] == 1 && wire[14] == period && wire[15] == theme);
            assert(coffee_decode(&loaded, wire, n));
            assert(loaded.home_period == period && loaded.theme == theme && loaded.count == 1);
            assert(memcmp(wire + 48, old_blob + 48, 12) == 0);
        }
    }
    for (unsigned offset = 14; offset <= 15; ++offset) {
        size_t n = coffee_encode(&d, wire, sizeof(wire));
        wire[offset] = offset == 14 ? COFFEE_PERIOD_COUNT : COFFEE_THEME_COUNT;
        update_crc(wire, n);
        candidate = loaded;
        assert(!coffee_decode(&loaded, wire, n));
        assert(memcmp(&candidate, &loaded, sizeof(loaded)) == 0);
    }
    d.home_period = COFFEE_PERIOD_COUNT;
    assert(!coffee_encode(&d, wire, sizeof(wire)));
    d.home_period = 0; d.theme = COFFEE_THEME_COUNT;
    assert(!coffee_encode(&d, wire, sizeof(wire)));
}

int main(void)
{
    natural_week_statistics();
    preferences_are_transactional();
    preferences_codec();
    assert(coffee_month_days(2000, 2) == 29 && coffee_month_days(2100, 2) == 28);
    assert(coffee_date_valid(20240229) && !coffee_date_valid(20230229));
    assert(!coffee_date_valid(20191231) && !coffee_date_valid(21000101));
    assert(coffee_date_step(20240228, 1) == 20240229);
    assert(coffee_date_step(20240229, 1) == 20240301);
    assert(coffee_date_step(20260101, -1) == 20251231);
    assert(coffee_date_step(20200101, -1) == 20200101);
    assert(coffee_date_month(20260131, 1) == 20260228);
    assert(coffee_date_month(20240331, -1) == 20240229);
    assert(coffee_weekday(20261006) == 1 && coffee_weekday(20240229) == 3);
    coffee_data_init(&d, 20261006);
    coffee_model_t m;
    coffee_model_init(&m, &d);
    assert(!m.clock_valid && m.page == COFFEE_HOME);
    coffee_model_key(&m, COFFEE_UP, 0);
    assert(m.page == COFFEE_HOME && m.focus == 4); /* Select the statistics card. */
    coffee_model_init(&m, &d);
    assert(coffee_model_key(&m, COFFEE_OK, 0) == COFFEE_ACTION_NONE && m.page == COFFEE_RECORD);
    assert(coffee_model_key(&m, COFFEE_OK, 0) == COFFEE_ACTION_NONE && m.page == COFFEE_DATE);
    m.date_field = 5;
    assert(coffee_model_key(&m, COFFEE_OK, 0) == COFFEE_ACTION_CLOCK);
    assert(coffee_prepare_change(&m, COFFEE_ACTION_CLOCK, &candidate));
    d = candidate;
    coffee_model_complete(&m, COFFEE_ACTION_CLOCK, true);
    assert(m.page == COFFEE_RECORD);
    assert(m.manually_set);
    assert(coffee_model_key(&m, COFFEE_OK, 0) == COFFEE_ACTION_RECORD);
    assert(coffee_prepare_change(&m, COFFEE_ACTION_RECORD, &candidate));
    assert(d.count == 0 && candidate.count == 1 && candidate.entries[0].manual_time);
    coffee_model_complete(&m, COFFEE_ACTION_RECORD, false);
    assert(m.page == COFFEE_RECORD && m.notice == COFFEE_NOTICE_FAILED && d.count == 0);
    d = candidate;
    coffee_model_complete(&m, COFFEE_ACTION_RECORD, true);
    assert(m.page == COFFEE_HOME && coffee_day_count(&d, 20261006) == 1);
    coffee_model_clock(&m, 20261007, 1439, true);
    assert(m.clock_valid && m.now_date == 20261007);
    m.page = COFFEE_RECORD; m.type = COFFEE_LATTE; m.draft_date = 20261007; m.draft_minute = 1439;
    m.manually_set = false; m.recipe = COFFEE_NO_RECIPE;
    assert(coffee_prepare_change(&m, COFFEE_ACTION_RECORD, &candidate));
    d = candidate;
    assert(coffee_day_entry(&d, 20261007, 0) == 1 && coffee_day_entry(&d, 20261007, 1) == -1);
    assert(coffee_month_count(&d, 20261007) == 2);
    size_t n = coffee_encode(&d, wire, sizeof(wire));
    assert(n > 0 && coffee_decode(&loaded, wire, n));
    assert(loaded.count == 2 && loaded.entries[1].type == COFFEE_LATTE && loaded.entries[1].minute == 1439);
    assert(!coffee_encode(&d, wire, n - 1));
    assert(!coffee_decode(&loaded, wire, n - 1));
    wire[n - 1] ^= 1; assert(!coffee_decode(&loaded, wire, n));
    assert(coffee_encode(&d, wire, sizeof(wire)) == n);
    wire[4] = 99; assert(!coffee_decode(&loaded, wire, n));
    coffee_data_init(&d, 20261006);
    d.count = COFFEE_CAPACITY;
    for (unsigned i = 0; i < d.count; ++i) d.entries[i] = (coffee_entry_t){20261006, 0, 0, COFFEE_HAND, COFFEE_NO_RECIPE, 0};
    assert(!coffee_prepare_change(&m, COFFEE_ACTION_RECORD, &candidate));
    m.cursor = 20261006; m.day_focus = 0;
    assert(coffee_prepare_change(&m, COFFEE_ACTION_DELETE, &candidate));
    assert(candidate.count == COFFEE_CAPACITY - 1 && d.count == COFFEE_CAPACITY);
    n = coffee_encode(&d, wire, sizeof(wire)); assert(n <= sizeof(wire) && coffee_decode(&loaded, wire, n));
    coffee_data_init(&d, 20261006); coffee_model_init(&m, &d);
    m.page = COFFEE_RECIPE; m.recipe = 0; m.focus = 0;
    coffee_model_key(&m, COFFEE_OK, 1000);
    assert(m.page == COFFEE_TIMER && coffee_timer_seconds(&m, 31000) == 30);
    assert(coffee_timer_stage(&m, 31000) == 1);
    coffee_model_key(&m, COFFEE_OK, 31000);
    assert(m.timer_paused && coffee_timer_seconds(&m, 91000) == 30);
    coffee_model_clock(&m, 20261101, 60, true); /* SNTP/wall-clock jump */
    coffee_model_key(&m, COFFEE_OK, 91000);
    assert(coffee_timer_seconds(&m, 121000) == 60);
    coffee_model_key(&m, COFFEE_UP, 121000);
    assert(m.page == COFFEE_RECORD && m.record_seconds == 60 && !m.timer_active);
    coffee_model_key(&m, COFFEE_BACK, 122000);
    assert(m.page == COFFEE_HOME);
    m.page = COFFEE_EDIT_RECIPE; m.recipe = 0; m.draft_recipe = d.recipes[0];
    m.focus = 0; coffee_model_key(&m, COFFEE_OK, 0); coffee_model_key(&m, COFFEE_DOWN, 0);
    assert(m.draft_recipe.grams == 16 && m.editing);
    coffee_model_key(&m, COFFEE_OK, 0); m.focus = 4;
    assert(coffee_model_key(&m, COFFEE_OK, 0) == COFFEE_ACTION_RECIPE);
    assert(coffee_prepare_change(&m, COFFEE_ACTION_RECIPE, &candidate) && candidate.recipes[0].grams == 16);
    assert(coffee_stage_water(&candidate.recipes[0], 3) == 240);
    candidate.recipes[0].seconds = 0; assert(!coffee_recipe_valid(&candidate.recipes[0]));
    assert(!coffee_encode(&candidate, wire, sizeof(wire)));
    coffee_model_init(&m, &d);
    coffee_model_key(&m, COFFEE_OK, 0);
    coffee_model_clock(&m, 20271008, 615, true);
    assert(m.draft_date == 20271008 && m.draft_minute == 615);
    assert(coffee_prepare_change(&m, COFFEE_ACTION_RECORD, &candidate));
    assert(candidate.entries[0].date == 20271008 && !candidate.entries[0].manual_time);
    coffee_model_key(&m, COFFEE_PREV_MONTH, 0);
    m.date_field = 2; m.editing = true; coffee_model_key(&m, COFFEE_DOWN, 0);
    assert(m.draft_date == 20271009);
    coffee_model_key(&m, COFFEE_BACK, 0);
    assert(m.page == COFFEE_RECORD && m.draft_date == 20271008 && m.draft_minute == 615);
    assert(coffee_prepare_change(&m, COFFEE_ACTION_RECORD, &candidate) && !candidate.entries[0].manual_time);
    coffee_model_init(&m, &d);
    coffee_model_key(&m, COFFEE_OK, 0);
    coffee_model_key(&m, COFFEE_PREV_MONTH, 0);
    coffee_model_clock(&m, 20271009, 720, true);
    assert(!m.draft_clock_valid);
    coffee_model_key(&m, COFFEE_BACK, 0);
    assert(m.draft_clock_valid && m.draft_date == 20271009 && m.draft_minute == 720);
    assert(coffee_prepare_change(&m, COFFEE_ACTION_RECORD, &candidate) && !candidate.entries[0].manual_time);
    m.page = COFFEE_FORGET; m.focus = 0;
    assert(coffee_model_key(&m, COFFEE_OK, 0) == COFFEE_ACTION_NONE && m.page == COFFEE_SETTINGS && m.focus == 3);
    m.page = COFFEE_FORGET; m.focus = 1;
    assert(coffee_model_key(&m, COFFEE_OK, 0) == COFFEE_ACTION_FORGET);
    puts("Coffee dates/calendar, transactional records, versioned CRC codec, recipe and monotonic timer: PASS");
    return 0;
}
