#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "coffee_store.h"
#include "nvs_flash.h"

/* Setters model IDF's immediate writes: commit failure does not roll back. */
static struct {
    uint8_t blob[COFFEE_WIRE_MAX + 1];
    size_t length;
    bool exists, write_on_set_failure;
    esp_err_t init_error, open_error, size_error, read_error, set_error, commit_error;
    unsigned init_calls, open_calls, size_calls, read_calls, set_calls, commit_calls, close_calls, erase_calls;
    bool change_read_length;
} fake;
static coffee_data_t data, expected, saved;

esp_err_t nvs_flash_init(void) { ++fake.init_calls; return fake.init_error; }
esp_err_t nvs_open(const char *name, nvs_open_mode_t mode, nvs_handle_t *handle)
{
    ++fake.open_calls;
    assert(strcmp(name, "coffee_notes") == 0 && mode == NVS_READWRITE);
    if (fake.open_error != ESP_OK) return fake.open_error;
    *handle = 17;
    return ESP_OK;
}
esp_err_t nvs_get_blob(nvs_handle_t handle, const char *key, void *value, size_t *length)
{
    assert(handle == 17 && strcmp(key, "journal") == 0);
    if (!value) {
        ++fake.size_calls;
        if (fake.size_error != ESP_OK) return fake.size_error;
        if (!fake.exists) return ESP_ERR_NVS_NOT_FOUND;
        *length = fake.length;
        return ESP_OK;
    }
    ++fake.read_calls;
    if (fake.read_error != ESP_OK) return fake.read_error;
    assert(*length >= fake.length);
    memcpy(value, fake.blob, fake.length);
    *length = fake.length + (fake.change_read_length ? 1 : 0);
    return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t handle, const char *key, const void *value, size_t length)
{
    assert(handle == 17 && strcmp(key, "journal") == 0);
    ++fake.set_calls;
    if (fake.set_error == ESP_OK || fake.write_on_set_failure) {
        assert(length <= sizeof(fake.blob));
        memcpy(fake.blob, value, length);
        fake.length = length;
        fake.exists = true;
    }
    return fake.set_error;
}
esp_err_t nvs_commit(nvs_handle_t handle) { assert(handle == 17); ++fake.commit_calls; return fake.commit_error; }
void nvs_close(nvs_handle_t handle) { assert(handle == 17); ++fake.close_calls; }
esp_err_t nvs_flash_erase(void) { ++fake.erase_calls; return ESP_OK; }
esp_err_t nvs_erase_key(nvs_handle_t handle, const char *key) { (void)handle; (void)key; ++fake.erase_calls; return ESP_OK; }
esp_err_t nvs_erase_all(nvs_handle_t handle) { (void)handle; ++fake.erase_calls; return ESP_OK; }

static void reset(void)
{
    memset(&fake, 0, sizeof(fake));
    coffee_data_init(&data, 20261006);
    expected = data;
}
static void assert_unchanged(void) { assert(memcmp(&data, &expected, sizeof(data)) == 0); }
static void assert_no_writes(void) { assert(fake.set_calls == 0 && fake.commit_calls == 0 && fake.erase_calls == 0); }
static void assert_data_equal(const coffee_data_t *left, const coffee_data_t *right)
{
    /* The wire format excludes compiler padding in entry structs. */
    assert(left->count == right->count && left->last_date == right->last_date && left->last_minute == right->last_minute);
    assert(left->home_period == right->home_period && left->theme == right->theme);
    for (unsigned i = 0; i < COFFEE_RECIPE_COUNT; ++i) {
        assert(left->recipes[i].grams == right->recipes[i].grams);
        assert(left->recipes[i].water_ml == right->recipes[i].water_ml);
        assert(left->recipes[i].degrees == right->recipes[i].degrees);
        assert(left->recipes[i].seconds == right->recipes[i].seconds);
    }
    for (unsigned i = 0; i < left->count; ++i) {
        assert(left->entries[i].date == right->entries[i].date);
        assert(left->entries[i].minute == right->entries[i].minute);
        assert(left->entries[i].duration == right->entries[i].duration);
        assert(left->entries[i].type == right->entries[i].type);
        assert(left->entries[i].recipe == right->entries[i].recipe);
        assert(left->entries[i].manual_time == right->entries[i].manual_time);
    }
}
static void populate(void)
{
    data.home_period = COFFEE_WEEK; data.theme = COFFEE_THEME_COUNT - 1;
    data.count = 3;
    data.last_date = 20261005;
    data.last_minute = 1234;
    data.recipes[1] = (coffee_recipe_t){22,330,96,220};
    data.entries[0] = (coffee_entry_t){20261005,555,185,COFFEE_HAND,1,0};
    data.entries[1] = (coffee_entry_t){20261001,755,0,COFFEE_LATTE,COFFEE_NO_RECIPE,1};
    data.entries[2] = (coffee_entry_t){20260930,859,0,COFFEE_COLD,COFFEE_NO_RECIPE,1};
}
static void seed_blob(void)
{
    populate();
    fake.length = coffee_encode(&data, fake.blob, sizeof(fake.blob));
    assert(fake.length != 0);
    fake.exists = true;
    saved = data;
    coffee_data_init(&data, 20261231);
    expected = data;
}
static void update_crc(void)
{
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0; i < fake.length; ++i) {
        if (i >= 44 && i < 48) continue;
        crc ^= fake.blob[i];
        for (unsigned bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U)));
    }
    crc = ~crc;
    for (unsigned i = 0; i < 4; ++i) fake.blob[44 + i] = (uint8_t)(crc >> (8 * i));
}

static void missing_keeps_defaults(void)
{
    reset(); coffee_store_t store = {0};
    assert(coffee_store_init(&store, &data) == ESP_OK);
    assert(store.ready && !store.corrupt && !store.read_failed);
    assert(fake.init_calls == 1 && fake.open_calls == 1 && fake.size_calls == 1 && fake.read_calls == 0);
    assert_unchanged(); assert_no_writes();
}
static void saves_and_loads_all_fields(void)
{
    reset(); coffee_store_t store = {0};
    assert(coffee_store_init(&store, &data) == ESP_OK);
    populate(); saved = data;
    assert(coffee_store_save(&store, &data) == ESP_OK);
    assert(store.ready && fake.set_calls == 1 && fake.commit_calls == 1 && fake.erase_calls == 0);
    unsigned writes = fake.set_calls;
    coffee_data_init(&data, 20261231);
    coffee_store_t restarted = {0};
    assert(coffee_store_init(&restarted, &data) == ESP_OK);
    assert(restarted.ready); assert_data_equal(&data, &saved);
    assert(fake.set_calls == writes && fake.commit_calls == 1 && fake.erase_calls == 0);
    assert(fake.init_calls == 2); /* NVS initialization is safe across owners. */
}
static void loads_without_writing(void)
{
    reset(); seed_blob(); coffee_store_t store = {0};
    assert(coffee_store_init(&store, &data) == ESP_OK);
    assert(store.ready); assert_data_equal(&data, &saved);
    assert(fake.size_calls == 1 && fake.read_calls == 1); assert_no_writes();
}
static void full_capacity_roundtrip(void)
{
    reset(); coffee_store_t store = {0};
    assert(coffee_store_init(&store, &data) == ESP_OK);
    data.count = COFFEE_CAPACITY;
    for (unsigned i = 0; i < COFFEE_CAPACITY; ++i) {
        data.entries[i] = (coffee_entry_t){20261006,(uint16_t)i,0,(uint8_t)(i % COFFEE_TYPE_COUNT),COFFEE_NO_RECIPE,1};
    }
    saved = data;
    assert(coffee_store_save(&store, &data) == ESP_OK && fake.length == COFFEE_WIRE_MAX);
    coffee_data_init(&data, 20261231); coffee_store_t restarted = {0};
    assert(coffee_store_init(&restarted, &data) == ESP_OK);
    assert_data_equal(&data, &saved);
    assert(fake.set_calls == 1 && fake.commit_calls == 1 && fake.erase_calls == 0);
}
static void corruption_blocks_overwrite(void)
{
    for (unsigned corruption = 0; corruption < 10; ++corruption) {
        reset(); seed_blob(); coffee_store_t store = {0};
        if (corruption == 0) fake.blob[44] ^= 1; /* CRC. */
        if (corruption == 1) { fake.blob[56] = COFFEE_TYPE_COUNT; update_crc(); } /* Valid CRC, invalid entry. */
        if (corruption == 2) { fake.blob[4] = 2; update_crc(); } /* Unknown version. */
        if (corruption == 3) fake.length = COFFEE_WIRE_MAX + 1;
        if (corruption == 4) fake.length = 0;
        if (corruption == 5) fake.length = 47;
        if (corruption == 6) --fake.length;
        if (corruption == 7) ++fake.length;
        if (corruption == 8) { fake.blob[14] = COFFEE_PERIOD_COUNT; update_crc(); }
        if (corruption == 9) { fake.blob[15] = COFFEE_THEME_COUNT; update_crc(); }
        assert(coffee_store_init(&store, &data) != ESP_OK);
        assert(!store.ready && store.corrupt);
        assert_unchanged();
        assert(coffee_store_save(&store, &data) == ESP_ERR_INVALID_STATE);
        assert_no_writes();
        if (corruption >= 3 && corruption <= 5) assert(fake.read_calls == 0);
    }
}
static void init_failure_never_erases(void)
{
    const esp_err_t failures[] = {ESP_FAIL, ESP_ERR_NVS_NO_FREE_PAGES, ESP_ERR_NVS_NEW_VERSION_FOUND};
    for (unsigned i = 0; i < sizeof(failures) / sizeof(failures[0]); ++i) {
        reset(); coffee_store_t store = {0}; fake.init_error = failures[i];
        assert(coffee_store_init(&store, &data) == failures[i]);
        assert(!store.ready && store.read_failed && fake.open_calls == 0);
        assert_unchanged(); assert_no_writes();
        assert(coffee_store_save(&store, &data) == ESP_ERR_INVALID_STATE);
    }
}
static void open_and_read_failures_preserve_data(void)
{
    for (unsigned stage = 0; stage < 3; ++stage) {
        reset(); seed_blob(); coffee_store_t store = {0};
        if (stage == 0) fake.open_error = ESP_ERR_NO_MEM;
        if (stage == 1) fake.size_error = ESP_ERR_NO_MEM;
        if (stage == 2) fake.read_error = ESP_ERR_NO_MEM;
        assert(coffee_store_init(&store, &data) == ESP_ERR_NO_MEM);
        assert(!store.ready && store.read_failed);
        assert_unchanged(); assert_no_writes();
        assert(coffee_store_save(&store, &data) == ESP_ERR_INVALID_STATE);
    }
}
static void inconsistent_read_length_is_rejected(void)
{
    reset(); seed_blob(); fake.change_read_length = true; coffee_store_t store = {0};
    assert(coffee_store_init(&store, &data) == ESP_ERR_INVALID_SIZE);
    assert(!store.ready && store.corrupt && fake.read_calls == 1);
    assert_unchanged(); assert_no_writes();
}
static void set_failure_blocks_retry_without_commit(void)
{
    for (unsigned ambiguous = 0; ambiguous < 2; ++ambiguous) {
        reset(); coffee_store_t store = {0};
        assert(coffee_store_init(&store, &data) == ESP_OK);
        populate(); expected = data; fake.set_error = ESP_ERR_NO_MEM;
        fake.write_on_set_failure = ambiguous;
        assert(coffee_store_save(&store, &data) == ESP_ERR_NO_MEM);
        assert(!store.ready && fake.set_calls == 1 && fake.commit_calls == 0);
        fake.set_error = ESP_OK;
        assert(coffee_store_save(&store, &data) == ESP_ERR_INVALID_STATE);
        assert(fake.set_calls == 1 && fake.commit_calls == 0 && fake.erase_calls == 0);
        assert_unchanged();
    }
}
static void commit_failure_blocks_retry_even_if_written(void)
{
    reset(); coffee_store_t store = {0};
    assert(coffee_store_init(&store, &data) == ESP_OK);
    populate(); expected = data; fake.commit_error = ESP_FAIL;
    assert(coffee_store_save(&store, &data) == ESP_FAIL);
    assert(!store.ready && fake.set_calls == 1 && fake.commit_calls == 1 && fake.exists);
    fake.commit_error = ESP_OK;
    assert(coffee_store_save(&store, &data) == ESP_ERR_INVALID_STATE);
    assert(fake.set_calls == 1 && fake.commit_calls == 1 && fake.erase_calls == 0);
    assert_unchanged();
    coffee_store_t restarted = {0}; coffee_data_init(&data, 20261231);
    assert(coffee_store_init(&restarted, &data) == ESP_OK);
    assert_data_equal(&data, &expected);
}
static void invalid_model_has_no_writes(void)
{
    reset(); coffee_store_t store = {0};
    assert(coffee_store_init(&store, &data) == ESP_OK);
    data.count = COFFEE_CAPACITY + 1;
    assert(coffee_store_save(&store, &data) == ESP_ERR_INVALID_ARG);
    assert(store.ready); assert_no_writes();
    coffee_data_init(&data, 20261006); data.recipes[0].grams = 0;
    assert(coffee_store_save(&store, &data) == ESP_ERR_INVALID_ARG);
    assert(store.ready); assert_no_writes();
}
static void preferences_roundtrip_preserves_records(void)
{
    for (unsigned period = 0; period < COFFEE_PERIOD_COUNT; ++period) {
        for (unsigned theme = 0; theme < COFFEE_THEME_COUNT; ++theme) {
            reset(); seed_blob(); coffee_store_t store = {0};
            assert(coffee_store_init(&store, &data) == ESP_OK);
            assert_data_equal(&data, &saved); saved = data; /* Normalize excluded struct padding after decode. */
            coffee_model_t model; coffee_model_init(&model, &data);
            model.page = COFFEE_THEMES;
            model.draft_period = (uint8_t)period; model.draft_theme = (uint8_t)theme;
            assert(coffee_prepare_change(&model, COFFEE_ACTION_PREFERENCES, &expected));
            assert(coffee_store_save(&store, &expected) == ESP_OK);
            data = expected;
            coffee_model_complete(&model, COFFEE_ACTION_PREFERENCES, true);
            assert(model.page == COFFEE_SETTINGS && model.focus == 2);
            assert(data.count == saved.count && memcmp(data.entries, saved.entries, sizeof(data.entries)) == 0);
            coffee_data_init(&data, 20261231); coffee_store_t restarted = {0};
            assert(coffee_store_init(&restarted, &data) == ESP_OK);
            assert_data_equal(&data, &expected);
            assert(fake.set_calls == 1 && fake.commit_calls == 1 && fake.erase_calls == 0);
        }
    }
}
static void preferences_failure_keeps_durable_state(void)
{
    for (unsigned stage = 0; stage < 2; ++stage) {
        reset(); seed_blob(); coffee_store_t store = {0};
        assert(coffee_store_init(&store, &data) == ESP_OK);
        coffee_model_t model; coffee_model_init(&model, &data);
        model.focus = 4;
        assert(coffee_model_key(&model, COFFEE_OK, 0) == COFFEE_ACTION_PREFERENCES);
        expected = data;
        assert(coffee_prepare_change(&model, COFFEE_ACTION_PREFERENCES, &saved));
        if (stage == 0) fake.set_error = ESP_FAIL;
        else fake.commit_error = ESP_FAIL;
        assert(coffee_store_save(&store, &saved) == ESP_FAIL);
        coffee_model_complete(&model, COFFEE_ACTION_PREFERENCES, false);
        assert(model.page == COFFEE_HOME && model.focus == 4 && model.notice == COFFEE_NOTICE_FAILED);
        assert_unchanged();
        /* NVS can already contain the write after commit failure; only RAM stays unchanged. */
        assert(!store.ready && fake.set_calls == 1 && fake.commit_calls == stage && fake.erase_calls == 0);
    }
}
static void legacy_reserved_preferences_load_defaults(void)
{
    reset(); seed_blob();
    fake.blob[14] = fake.blob[15] = 0; update_crc();
    saved.home_period = COFFEE_TODAY; saved.theme = 0;
    coffee_store_t store = {0};
    assert(coffee_store_init(&store, &data) == ESP_OK);
    assert_data_equal(&data, &saved); assert_no_writes();
}
static void invalid_preferences_have_no_writes(void)
{
    reset(); coffee_store_t store = {0};
    assert(coffee_store_init(&store, &data) == ESP_OK);
    populate(); data.home_period = COFFEE_PERIOD_COUNT;
    assert(coffee_store_save(&store, &data) == ESP_ERR_INVALID_ARG);
    assert(store.ready); assert_no_writes();
    data.home_period = COFFEE_MONTH; data.theme = COFFEE_THEME_COUNT;
    assert(coffee_store_save(&store, &data) == ESP_ERR_INVALID_ARG);
    assert(store.ready); assert_no_writes();
}
static void invalid_arguments_are_rejected(void)
{
    reset(); coffee_store_t store = {0};
    assert(coffee_store_init(NULL, &data) == ESP_ERR_INVALID_ARG);
    assert(coffee_store_init(&store, NULL) == ESP_ERR_INVALID_ARG);
    assert(coffee_store_save(NULL, &data) == ESP_ERR_INVALID_ARG);
    assert(coffee_store_save(&store, NULL) == ESP_ERR_INVALID_ARG);
    assert(fake.init_calls == 0 && fake.open_calls == 0); assert_no_writes();
}

int main(int argc, char **argv)
{
    const struct { const char *name; void (*run)(void); } cases[] = {
        {"missing", missing_keeps_defaults}, {"roundtrip", saves_and_loads_all_fields},
        {"load", loads_without_writing}, {"capacity", full_capacity_roundtrip}, {"corrupt", corruption_blocks_overwrite},
        {"init_failure", init_failure_never_erases}, {"read_failure", open_and_read_failures_preserve_data},
        {"read_length", inconsistent_read_length_is_rejected}, {"set_failure", set_failure_blocks_retry_without_commit},
        {"commit_failure", commit_failure_blocks_retry_even_if_written}, {"invalid_model", invalid_model_has_no_writes},
        {"preferences", preferences_roundtrip_preserves_records},
        {"preferences_failure", preferences_failure_keeps_durable_state},
        {"legacy", legacy_reserved_preferences_load_defaults},
        {"invalid_preferences", invalid_preferences_have_no_writes},
        {"arguments", invalid_arguments_are_rejected}
    };
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        if (argc > 1 && strcmp(argv[1], cases[i].name)) continue;
        cases[i].run();
        printf("coffee store: %s PASS\n", cases[i].name);
    }
    return 0;
}
