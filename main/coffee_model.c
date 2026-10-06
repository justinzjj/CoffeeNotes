#include "coffee_model.h"
#include <string.h>

unsigned coffee_month_days(unsigned year, unsigned month)
{
    static const unsigned days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (month < 1 || month > 12) return 0;
    return days[month - 1] + (month == 2 && year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}
bool coffee_date_valid(uint32_t date)
{
    unsigned y = date / 10000, mo = date / 100 % 100, day = date % 100;
    return y >= 2020 && y <= 2099 && day >= 1 && day <= coffee_month_days(y, mo);
}
uint32_t coffee_date_step(uint32_t date, int delta)
{
    if (!coffee_date_valid(date)) return 20200101;
    unsigned y = date / 10000, mo = date / 100 % 100, d = date % 100;
    while (delta > 0 && date < 20991231) {
        if (++d > coffee_month_days(y, mo)) { d = 1; if (++mo > 12) { mo = 1; ++y; } }
        date = y * 10000 + mo * 100 + d; --delta;
    }
    while (delta < 0 && date > 20200101) {
        if (d == 1) { if (mo == 1) { mo = 12; --y; } else --mo; d = coffee_month_days(y, mo); }
        else --d;
        date = y * 10000 + mo * 100 + d; ++delta;
    }
    return date;
}
uint32_t coffee_date_month(uint32_t date, int delta)
{
    if (!coffee_date_valid(date)) return 20200101;
    int month_index = (int)(date / 10000 - 2020) * 12 + (int)(date / 100 % 100) - 1 + delta;
    if (month_index < 0) month_index = 0;
    if (month_index > 959) month_index = 959;
    unsigned y = 2020 + (unsigned)month_index / 12, mo = (unsigned)month_index % 12 + 1, d = date % 100;
    unsigned max = coffee_month_days(y, mo);
    return y * 10000 + mo * 100 + (d > max ? max : d);
}
unsigned coffee_weekday(uint32_t date)
{
    if (!coffee_date_valid(date)) return 0;
    unsigned y = date / 10000, mo = date / 100 % 100, days = 2 + date % 100 - 1;
    for (unsigned year = 2020; year < y; ++year) days += coffee_month_days(year, 2) == 29 ? 366 : 365;
    for (unsigned month = 1; month < mo; ++month) days += coffee_month_days(y, month);
    return days % 7;
}
unsigned coffee_day_count(const coffee_data_t *data, uint32_t date)
{
    unsigned count = 0;
    for (unsigned i = 0; i < data->count; ++i) if (data->entries[i].date == date) ++count;
    return count;
}
int coffee_day_entry(const coffee_data_t *data, uint32_t date, unsigned index)
{
    /* Most recent saved entry is first, including backfilled dates. */
    for (int i = (int)data->count - 1; i >= 0; --i) {
        if (data->entries[i].date == date) { if (!index) return i; --index; }
    }
    return -1;
}
unsigned coffee_month_count(const coffee_data_t *data, uint32_t date)
{
    unsigned count = 0;
    for (unsigned i = 0; i < data->count; ++i) if (data->entries[i].date / 100 == date / 100) ++count;
    return count;
}
bool coffee_recipe_valid(const coffee_recipe_t *r)
{
    return r->grams >= 5 && r->grams <= 40 && r->water_ml >= 50 && r->water_ml <= 600 &&
           r->degrees >= 70 && r->degrees <= 99 && r->seconds >= 60 && r->seconds <= 360;
}
void coffee_data_init(coffee_data_t *data, uint32_t seed_date)
{
    memset(data, 0, sizeof(*data));
    data->last_date = coffee_date_valid(seed_date) ? seed_date : 20200101;
    data->last_minute = 9 * 60;
    data->recipes[0] = (coffee_recipe_t){15,240,92,150};
    data->recipes[1] = (coffee_recipe_t){20,300,94,180};
    data->recipes[2] = (coffee_recipe_t){15,150,91,120};
}
void coffee_model_init(coffee_model_t *m, coffee_data_t *data)
{
    memset(m, 0, sizeof(*m));
    m->data = data; m->page = COFFEE_HOME;
    m->cursor = m->now_date = m->draft_date = data->last_date;
    m->now_minute = m->draft_minute = data->last_minute;
    m->recipe = COFFEE_NO_RECIPE;
}
void coffee_model_clock(coffee_model_t *m, uint32_t date, unsigned minute, bool synced)
{
    if (!coffee_date_valid(date) || minute >= 1440) return;
    bool first_sync = synced && !m->clock_valid;
    if (m->page == COFFEE_HOME && m->cursor == m->now_date) m->cursor = date;
    m->now_date = date; m->now_minute = (uint16_t)minute;
    if (synced) { m->clock_valid = true; m->clock_manual = false; }
    if (first_sync && m->page == COFFEE_RECORD && !m->manually_set) {
        m->draft_date = date; m->draft_minute = (uint16_t)minute;
        m->draft_clock_valid = true;
    }
}
uint32_t coffee_timer_seconds(const coffee_model_t *m, uint64_t now_ms)
{
    uint64_t ms = m->timer_accum;
    if (m->timer_active && !m->timer_paused && now_ms >= m->timer_anchor) ms += now_ms - m->timer_anchor;
    return (uint32_t)(ms / 1000 > 5999 ? 5999 : ms / 1000);
}
unsigned coffee_timer_stage(const coffee_model_t *m, uint64_t now_ms)
{
    if (m->recipe >= COFFEE_RECIPE_COUNT) return 0;
    unsigned seconds = coffee_timer_seconds(m, now_ms), target = m->data->recipes[m->recipe].seconds;
    if (seconds * 100 >= target * 70) return 3;
    if (seconds * 100 >= target * 45) return 2;
    if (seconds * 100 >= target * 20) return 1;
    return 0;
}
unsigned coffee_stage_water(const coffee_recipe_t *recipe, unsigned stage)
{
    const unsigned percent[] = {20,45,75,100};
    return (recipe->water_ml * percent[stage > 3 ? 3 : stage] + 50) / 100;
}
static unsigned cycle(unsigned value, unsigned count, int delta)
{
    return (unsigned)((int)value + (int)count + delta) % count;
}
static void record_begin(coffee_model_t *m, uint32_t date, bool from_timer)
{
    m->page = COFFEE_RECORD; m->type = COFFEE_HAND; m->focus = 0;
    m->draft_date = date; m->draft_minute = m->now_minute;
    m->draft_clock_valid = m->clock_valid;
    m->manually_set = m->clock_manual;
    if (m->clock_valid && date != m->now_date) m->manually_set = true;
    if (!from_timer) { m->record_seconds = 0; m->recipe = COFFEE_NO_RECIPE; }
}
static void date_begin(coffee_model_t *m, coffee_page_t back, bool set_clock)
{
    m->date_backup = m->draft_date; m->minute_backup = m->draft_minute;
    m->date_return = back; m->date_for_clock = set_clock || !m->clock_valid; m->page = COFFEE_DATE;
    m->date_field = 0; m->editing = false;
    if (set_clock) { m->draft_date = m->now_date; m->draft_minute = m->now_minute; }
}
static void date_adjust(coffee_model_t *m, int delta)
{
    unsigned y = m->draft_date / 10000, mo = m->draft_date / 100 % 100, d = m->draft_date % 100;
    if (m->date_field == 0) y = 2020 + cycle(y - 2020, 80, delta);
    if (m->date_field == 1) mo = 1 + cycle(mo - 1, 12, delta);
    unsigned max = coffee_month_days(y, mo);
    if (d > max) d = max;
    if (m->date_field == 2) d = 1 + cycle(d - 1, max, delta);
    if (m->date_field == 3) m->draft_minute = (uint16_t)(cycle(m->draft_minute / 60, 24, delta) * 60 + m->draft_minute % 60);
    if (m->date_field == 4) m->draft_minute = (uint16_t)(m->draft_minute / 60 * 60 + cycle(m->draft_minute % 60, 60, delta));
    m->draft_date = y * 10000 + mo * 100 + d;
}
static void recipe_adjust(coffee_model_t *m, int delta)
{
    uint16_t *field = NULL; unsigned min = 0, max = 0, step = 1;
    switch (m->focus) {
    case 0: field = &m->draft_recipe.grams; min = 5; max = 40; break;
    case 1: field = &m->draft_recipe.water_ml; min = 50; max = 600; step = 10; break;
    case 2: field = &m->draft_recipe.degrees; min = 70; max = 99; break;
    case 3: field = &m->draft_recipe.seconds; min = 60; max = 360; step = 5; break;
    default: return;
    }
    int changed = (int)*field + delta * (int)step;
    *field = (uint16_t)(changed < (int)min ? min : changed > (int)max ? max : (unsigned)changed);
}
coffee_action_t coffee_model_key(coffee_model_t *m, coffee_key_t key, uint64_t now_ms)
{
    m->notice = COFFEE_NOTICE_NONE;
    int delta = key == COFFEE_UP ? -1 : 1;
    bool move = key == COFFEE_UP || key == COFFEE_DOWN;
    if (key == COFFEE_BACK) {
        m->editing = false;
        switch (m->page) {
        case COFFEE_HOME: return COFFEE_ACTION_NONE;
        case COFFEE_DAY: m->page = COFFEE_CALENDAR; break;
        case COFFEE_DELETE: m->page = COFFEE_DAY; break;
        case COFFEE_RECIPE: m->page = COFFEE_RECIPES; m->focus = m->recipe; break;
        case COFFEE_EDIT_RECIPE: m->page = COFFEE_RECIPE; m->focus = 0; break;
        case COFFEE_TIMER: m->timer_active = false; m->page = COFFEE_RECIPE; m->focus = 0; break;
        case COFFEE_DATE:
            m->draft_date = m->date_backup; m->draft_minute = m->minute_backup;
            m->page = m->date_return;
            if (m->page == COFFEE_RECORD && !m->draft_clock_valid && !m->manually_set && m->clock_valid) {
                m->draft_date = m->now_date; m->draft_minute = m->now_minute; m->draft_clock_valid = true;
            }
            break;
        case COFFEE_NETWORK: m->page = COFFEE_SETTINGS; m->focus = 0; return COFFEE_ACTION_CANCEL_SETUP;
        case COFFEE_FORGET: m->page = COFFEE_SETTINGS; m->focus = 2; break;
        default: m->page = COFFEE_HOME; m->focus = 0; break;
        }
        return COFFEE_ACTION_NONE;
    }
    switch (m->page) {
    case COFFEE_HOME:
        if (move) m->focus = (uint16_t)cycle(m->focus, 4, delta);
        if (key == COFFEE_OK) {
            if (m->focus == 0) record_begin(m, m->now_date, false);
            else if (m->focus == 1) { m->page = COFFEE_CALENDAR; m->cursor = m->now_date; }
            else if (m->focus == 2) { m->page = COFFEE_RECIPES; m->focus = 0; }
            else { m->page = COFFEE_SETTINGS; m->focus = 0; }
        }
        break;
    case COFFEE_CALENDAR:
        if (move) m->cursor = coffee_date_step(m->cursor, delta);
        if (key == COFFEE_PREV_MONTH || key == COFFEE_NEXT_MONTH) m->cursor = coffee_date_month(m->cursor, key == COFFEE_PREV_MONTH ? -1 : 1);
        if (key == COFFEE_OK) { m->page = COFFEE_DAY; m->day_focus = 0; }
        break;
    case COFFEE_DAY: {
        unsigned count = coffee_day_count(m->data, m->cursor);
        if (move) m->day_focus = (uint16_t)cycle(m->day_focus, count + 1, delta);
        if (key == COFFEE_OK) {
            if (m->day_focus == count) record_begin(m, m->cursor, false);
            else { m->page = COFFEE_DELETE; m->focus = 0; }
        }
        break;
    }
    case COFFEE_RECORD:
        if (move) { m->type = (uint8_t)cycle(m->type, COFFEE_TYPE_COUNT, delta); if (m->type != COFFEE_HAND) { m->recipe = COFFEE_NO_RECIPE; m->record_seconds = 0; } }
        if (key == COFFEE_PREV_MONTH) date_begin(m, COFFEE_RECORD, false);
        if (key == COFFEE_OK) {
            if (!m->draft_clock_valid && !m->manually_set) date_begin(m, COFFEE_RECORD, false);
            else if (m->data->count >= COFFEE_CAPACITY) m->notice = COFFEE_NOTICE_FULL;
            else return COFFEE_ACTION_RECORD;
        }
        break;
    case COFFEE_RECIPES:
        if (move) m->focus = (uint16_t)cycle(m->focus, COFFEE_RECIPE_COUNT, delta);
        if (key == COFFEE_OK) { m->recipe = (uint8_t)m->focus; m->page = COFFEE_RECIPE; m->focus = 0; }
        break;
    case COFFEE_RECIPE:
        if (move) m->focus = (uint16_t)cycle(m->focus, 2, delta);
        if (key == COFFEE_OK) {
            if (m->focus == 0) { m->page = COFFEE_TIMER; m->timer_active = true; m->timer_paused = false; m->timer_accum = 0; m->timer_anchor = now_ms; }
            else { m->page = COFFEE_EDIT_RECIPE; m->draft_recipe = m->data->recipes[m->recipe]; m->focus = 0; m->editing = false; }
        }
        break;
    case COFFEE_EDIT_RECIPE:
        if (move) { if (m->editing) recipe_adjust(m, delta); else m->focus = (uint16_t)cycle(m->focus, 5, delta); }
        if (key == COFFEE_OK) { if (m->focus == 4) return COFFEE_ACTION_RECIPE; m->editing = !m->editing; }
        break;
    case COFFEE_TIMER:
        if (key == COFFEE_OK) {
            if (m->timer_paused) { m->timer_anchor = now_ms; m->timer_paused = false; }
            else { if (now_ms >= m->timer_anchor) m->timer_accum += now_ms - m->timer_anchor; m->timer_paused = true; }
        }
        if (key == COFFEE_UP) {
            m->record_seconds = (uint16_t)coffee_timer_seconds(m, now_ms);
            m->timer_active = false; record_begin(m, m->now_date, true);
        }
        break;
    case COFFEE_SETTINGS:
        if (move) m->focus = (uint16_t)cycle(m->focus, 3, delta);
        if (key == COFFEE_OK) {
            if (m->focus == 0) { m->page = COFFEE_NETWORK; return COFFEE_ACTION_SETUP; }
            if (m->focus == 1) date_begin(m, COFFEE_SETTINGS, true);
            if (m->focus == 2) { m->page = COFFEE_FORGET; m->focus = 0; }
        }
        break;
    case COFFEE_NETWORK: if (key == COFFEE_OK) return COFFEE_ACTION_SETUP; break;
    case COFFEE_DATE:
        if (move) { if (m->editing) date_adjust(m, delta); else m->date_field = (uint8_t)cycle(m->date_field, 6, delta); }
        if (key == COFFEE_OK) {
            if (m->date_field == 5) {
                if (m->date_for_clock) return COFFEE_ACTION_CLOCK;
                m->manually_set = true; m->draft_clock_valid = true; m->page = m->date_return; m->editing = false;
            } else m->editing = !m->editing;
        }
        break;
    case COFFEE_DELETE:
        if (move) m->focus = (uint16_t)cycle(m->focus, 2, delta);
        if (key == COFFEE_OK) { if (m->focus == 1) return COFFEE_ACTION_DELETE; m->page = COFFEE_DAY; }
        break;
    case COFFEE_FORGET:
        if (move) m->focus = (uint16_t)cycle(m->focus, 2, delta);
        if (key == COFFEE_OK) {
            bool confirmed = m->focus == 1;
            m->page = COFFEE_SETTINGS; m->focus = 2;
            if (confirmed) return COFFEE_ACTION_FORGET;
        }
        /* Keep cancellation selected by default. */
        break;
    }
    return COFFEE_ACTION_NONE;
}
static bool entry_valid(const coffee_entry_t *e)
{
    return coffee_date_valid(e->date) && e->minute < 1440 && e->duration <= 5999 &&
           e->type < COFFEE_TYPE_COUNT && (e->recipe == COFFEE_NO_RECIPE || (e->recipe < COFFEE_RECIPE_COUNT && e->type == COFFEE_HAND)) && e->manual_time <= 1;
}
static bool data_valid(const coffee_data_t *d)
{
    if (d->count > COFFEE_CAPACITY || !coffee_date_valid(d->last_date) || d->last_minute >= 1440) return false;
    for (unsigned i = 0; i < COFFEE_RECIPE_COUNT; ++i) if (!coffee_recipe_valid(&d->recipes[i])) return false;
    for (unsigned i = 0; i < d->count; ++i) if (!entry_valid(&d->entries[i])) return false;
    return true;
}
bool coffee_prepare_change(const coffee_model_t *m, coffee_action_t action, coffee_data_t *c)
{
    *c = *m->data;
    switch (action) {
    case COFFEE_ACTION_RECORD:
        if (c->count >= COFFEE_CAPACITY || (!m->draft_clock_valid && !m->manually_set)) return false;
        c->entries[c->count++] = (coffee_entry_t){m->draft_date,m->draft_minute,m->record_seconds,m->type,m->recipe,m->manually_set};
        c->last_date = m->now_date; c->last_minute = m->now_minute;
        break;
    case COFFEE_ACTION_DELETE: {
        int index = coffee_day_entry(c, m->cursor, m->day_focus);
        if (index < 0) return false;
        memmove(&c->entries[index], &c->entries[index + 1], (c->count - (unsigned)index - 1) * sizeof(coffee_entry_t)); --c->count;
        break;
    }
    case COFFEE_ACTION_RECIPE:
        if (m->recipe >= COFFEE_RECIPE_COUNT) return false;
        c->recipes[m->recipe] = m->draft_recipe; break;
    case COFFEE_ACTION_CLOCK:
        c->last_date = m->draft_date; c->last_minute = m->draft_minute; break;
    default: return false;
    }
    return data_valid(c);
}
void coffee_model_complete(coffee_model_t *m, coffee_action_t action, bool success)
{
    m->notice = success ? COFFEE_NOTICE_SAVED : COFFEE_NOTICE_FAILED;
    if (!success) return;
    if (action == COFFEE_ACTION_RECORD) { m->page = COFFEE_HOME; m->focus = 0; }
    if (action == COFFEE_ACTION_DELETE) { m->page = COFFEE_DAY; m->day_focus = 0; }
    if (action == COFFEE_ACTION_RECIPE) { m->page = COFFEE_RECIPE; m->focus = 0; m->editing = false; }
    if (action == COFFEE_ACTION_CLOCK) {
        m->now_date = m->cursor = m->draft_date; m->now_minute = m->draft_minute;
        m->clock_valid = true; m->clock_manual = true; m->page = m->date_return;
        m->manually_set = m->date_return == COFFEE_RECORD;
        if (m->date_return == COFFEE_RECORD) m->draft_clock_valid = true;
        m->focus = m->page == COFFEE_SETTINGS ? 1 : 0; m->editing = false;
    }
}
static void put16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put32(uint8_t *p, uint32_t v) { for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (i * 8)); }
static uint16_t get16(const uint8_t *p) { return (uint16_t)(p[0] | (uint16_t)p[1] << 8); }
static uint32_t get32(const uint8_t *p) { return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint32_t checksum(const uint8_t *p, size_t n)
{
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0; i < n; ++i) {
        if (i >= 44 && i < 48) continue;
        crc ^= p[i];
        for (unsigned bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U)));
    }
    return ~crc;
}
size_t coffee_encode(const coffee_data_t *d, uint8_t *out, size_t capacity)
{
    size_t n = 48 + (size_t)d->count * 12;
    if (!data_valid(d) || capacity < n) return 0;
    memset(out, 0, n); memcpy(out, "CFN1", 4); out[4] = 1;
    put16(out + 6, d->count); put32(out + 8, d->last_date); put16(out + 12, d->last_minute);
    for (unsigned i = 0; i < COFFEE_RECIPE_COUNT; ++i) {
        uint8_t *p = out + 16 + i * 8; const coffee_recipe_t *r = &d->recipes[i];
        put16(p, r->grams); put16(p + 2, r->water_ml); put16(p + 4, r->degrees); put16(p + 6, r->seconds);
    }
    for (unsigned i = 0; i < d->count; ++i) {
        uint8_t *p = out + 48 + i * 12; const coffee_entry_t *e = &d->entries[i];
        put32(p, e->date); put16(p + 4, e->minute); put16(p + 6, e->duration); p[8] = e->type; p[9] = e->recipe; p[10] = e->manual_time;
    }
    put32(out + 44, checksum(out, n)); return n;
}
static coffee_recipe_t read_recipe(const uint8_t *p) { return (coffee_recipe_t){get16(p),get16(p + 2),get16(p + 4),get16(p + 6)}; }
static coffee_entry_t read_entry(const uint8_t *p) { return (coffee_entry_t){get32(p),get16(p + 4),get16(p + 6),p[8],p[9],p[10]}; }
bool coffee_decode(coffee_data_t *d, const uint8_t *input, size_t n)
{
    if (n < 48 || n > COFFEE_WIRE_MAX || memcmp(input, "CFN1", 4) || input[4] != 1) return false;
    unsigned count = get16(input + 6);
    if (count > COFFEE_CAPACITY || n != 48 + count * 12 || get32(input + 44) != checksum(input, n) ||
        !coffee_date_valid(get32(input + 8)) || get16(input + 12) >= 1440) return false;
    for (unsigned i = 0; i < COFFEE_RECIPE_COUNT; ++i) { coffee_recipe_t r = read_recipe(input + 16 + i * 8); if (!coffee_recipe_valid(&r)) return false; }
    for (unsigned i = 0; i < count; ++i) { coffee_entry_t e = read_entry(input + 48 + i * 12); if (!entry_valid(&e)) return false; }
    /* Validate fully before modifying the caller's durable state. */
    memset(d, 0, sizeof(*d)); d->count = (uint16_t)count; d->last_date = get32(input + 8); d->last_minute = get16(input + 12);
    for (unsigned i = 0; i < COFFEE_RECIPE_COUNT; ++i) d->recipes[i] = read_recipe(input + 16 + i * 8);
    for (unsigned i = 0; i < count; ++i) d->entries[i] = read_entry(input + 48 + i * 12);
    return true;
}
