#include "coffee_ui.h"
#include "coffee_glyphs.h"
#include "lvgl.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

LV_FONT_DECLARE(coffee_font_14);
static uint16_t pixels[240 * 320], buffer[240 * 40];
static coffee_data_t data;
static const char *render_name;
static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *bytes)
{
    unsigned w = (unsigned)(area->x2 - area->x1 + 1);
    for (int y = area->y1; y <= area->y2; ++y) memcpy(pixels + y * 240 + area->x1, bytes + (y - area->y1) * w * 2, w * 2);
    lv_display_flush_ready(display);
}
static uint32_t codepoint(const unsigned char **p)
{
    uint32_t c = *(*p)++;
    if (c < 128) return c;
    unsigned remaining = c < 0xE0 ? 1 : c < 0xF0 ? 2 : 3;
    c &= remaining == 1 ? 0x1F : remaining == 2 ? 0x0F : 7;
    while (remaining--) c = (c << 6) | (*(*p)++ & 0x3F);
    return c;
}
static void audit(lv_obj_t *parent)
{
    for (uint32_t i = 0; i < lv_obj_get_child_count(parent); ++i) {
        lv_obj_t *obj = lv_obj_get_child(parent, (int32_t)i);
        if (lv_obj_check_type(obj, &lv_label_class)) {
            const lv_font_t *font = lv_obj_get_style_text_font(obj, LV_PART_MAIN);
            const char *s = lv_label_get_text(obj); const unsigned char *p = (const unsigned char *)s;
            while (*p) {
                uint32_t c = codepoint(&p); if (c == '\n') continue;
                lv_font_glyph_dsc_t glyph = {0};
                if (!lv_font_get_glyph_dsc(font, &glyph, c, 0) || glyph.is_placeholder) {
                    fprintf(stderr, "%s missing U+%04X in %s\n", render_name, c, s); exit(1);
                }
            }
            lv_point_t natural;
            lv_text_get_size(&natural, s, font, lv_obj_get_style_text_letter_space(obj, 0), lv_obj_get_style_text_line_space(obj, 0), LV_COORD_MAX, LV_TEXT_FLAG_NONE);
            if (natural.x > lv_obj_get_width(obj)) { fprintf(stderr, "%s wraps/clips: %s (%ld > %ld)\n", render_name,s,(long)natural.x,(long)lv_obj_get_width(obj)); exit(1); }
            lv_area_t a, par; lv_obj_get_coords(obj, &a); lv_obj_get_coords(parent, &par);
            if (a.x1 < 0 || a.y1 < 0 || a.x2 >= 240 || a.y2 >= 320 || a.x1 < par.x1 || a.y1 < par.y1 || a.x2 > par.x2 || a.y2 > par.y2) {
                fprintf(stderr, "%s text out of bounds: %s (%ld,%ld..%ld,%ld) parent(%ld,%ld..%ld,%ld)\n",render_name,s,(long)a.x1,(long)a.y1,(long)a.x2,(long)a.y2,(long)par.x1,(long)par.y1,(long)par.x2,(long)par.y2); exit(1);
            }
        }
        audit(obj);
    }
}
static lv_obj_t *find_label(lv_obj_t *parent, const char *value)
{
    for (uint32_t i = 0; i < lv_obj_get_child_count(parent); ++i) {
        lv_obj_t *child = lv_obj_get_child(parent, (int32_t)i);
        if (lv_obj_check_type(child, &lv_label_class) && !strcmp(lv_label_get_text(child), value)) return child;
        lv_obj_t *found = find_label(child, value);
        if (found) return found;
    }
    return NULL;
}
static void audit_heatmap(void)
{
    /* Fixture: day 4=zero, day 1=one, day 2=two, day 3/6=three cups. */
    const char *days[] = {"4","1","2","3"};
    lv_color_t levels[4];
    for (unsigned i = 0; i < 4; ++i) {
        lv_obj_t *cell = find_label(lv_screen_active(), days[i]); assert(cell);
        levels[i] = lv_obj_get_style_bg_color(cell, LV_PART_MAIN);
        for (unsigned j = 0; j < i; ++j) assert(!lv_color_eq(levels[i], levels[j]));
    }
    lv_obj_t *selected = find_label(lv_screen_active(), "6"); assert(selected);
    assert(lv_obj_get_style_outline_width(selected, LV_PART_MAIN) == 2);
    assert(lv_color_eq(lv_obj_get_style_bg_color(selected, LV_PART_MAIN), levels[3]));
    assert(!find_label(lv_screen_active(), "6."));
}
static bool inside_corner(int x, int y)
{
    int cx = x < 30 ? 30 : x >= 210 ? 209 : x;
    int cy = y < 30 ? 30 : y >= 290 ? 289 : y;
    int dx = x - cx, dy = y - cy;
    return dx * dx + dy * dy <= 30 * 30;
}
static void render(const char *name, coffee_model_t *model, coffee_ui_info_t *info, uint64_t now)
{
    render_name = name;
    if (name) { printf("Rendering %s\n", name); fflush(stdout); }
    coffee_ui_render(model, info, now); lv_obj_update_layout(lv_screen_active()); audit(lv_screen_active()); lv_refr_now(NULL);
    if (!name) return;
    char path[80]; snprintf(path,sizeof(path),"%s.ppm",name); FILE *f = fopen(path,"wb"); assert(f);
    fprintf(f,"P6\n240 320\n255\n");
    for (int y = 0; y < 320; ++y) for (int x = 0; x < 240; ++x) {
        uint16_t c = inside_corner(x,y) ? pixels[y * 240 + x] : 0;
        unsigned char rgb[] = {(unsigned char)((c >> 11) * 255 / 31),(unsigned char)(((c >> 5) & 63) * 255 / 63),(unsigned char)((c & 31) * 255 / 31)};
        assert(fwrite(rgb,1,3,f) == 3);
    }
    fclose(f);
}
int main(void)
{
    lv_init(); lv_display_t *display = lv_display_create(240,320); assert(display);
    lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display,buffer,NULL,sizeof(buffer),LV_DISPLAY_RENDER_MODE_PARTIAL); lv_display_set_flush_cb(display,flush);
    assert(coffee_ui_verify_fonts());
    lv_font_glyph_dsc_t missing = {0};
    assert(!lv_font_get_glyph_dsc(&coffee_font_14,&missing,0x9F98,0) || missing.is_placeholder);
    assert(coffee_ui_create());
    coffee_data_init(&data,20261006);
    data.count = 7;
    for (unsigned i = 0; i < data.count; ++i) data.entries[i] = (coffee_entry_t){20261006, (uint16_t)(550 + i * 60),150,(uint8_t)(i % 6),COFFEE_NO_RECIPE,0};
    data.entries[0].recipe = 0;
    coffee_model_t model; coffee_model_init(&model,&data);
    coffee_ui_info_t info = {.battery = 100,.storage_ready = true,.input_ready = true};
    render("home-unconfirmed",&model,&info,0);
    coffee_model_clock(&model,20261006,630,true); render("home",&model,&info,0);
    model.page = COFFEE_CALENDAR; render("calendar",&model,&info,0);
    model.cursor = 20200831; render("calendar-six-weeks",&model,&info,0);
    model.cursor = 20261006; model.page = COFFEE_DAY; render("day",&model,&info,0);
    model.day_focus = 7; render("day-scrolled",&model,&info,0);
    model.page = COFFEE_RECORD; model.recipe = 0; model.type = 0; model.draft_date = 20261006; model.draft_minute = 1439; model.draft_clock_valid = true; model.record_seconds = 150;
    render("record",&model,&info,0);
    assert(!find_label(lv_screen_active(), "已校时 可直接保存"));
    for (unsigned i = 0; i < COFFEE_TYPE_COUNT; ++i) { model.type = (uint8_t)i; render(NULL,&model,&info,0); }
    model.page = COFFEE_RECIPES; model.focus = 1; render("recipes",&model,&info,0);
    for (unsigned i = 0; i < 3; ++i) { model.page = COFFEE_RECIPE; model.recipe = (uint8_t)i; model.focus = 0; render(i == 0 ? "recipe" : NULL,&model,&info,0); }
    model.recipe = 0; model.page = COFFEE_EDIT_RECIPE; model.draft_recipe = (coffee_recipe_t){40,600,99,360}; model.focus = 4; render("recipe-editor",&model,&info,0);
    model.page = COFFEE_TIMER; model.timer_active = true; model.timer_anchor = 0; model.timer_accum = 0;
    render("timer-bloom",&model,&info,0); render("timer",&model,&info,70000); render("timer-complete",&model,&info,151000);
    model.timer_paused = true; model.timer_accum = 61000; render("timer-paused",&model,&info,70000); model.timer_active = false;
    model.page = COFFEE_SETTINGS; model.focus = 0; render("settings",&model,&info,0);
    model.clock_valid = false; render("settings-unconfirmed",&model,&info,0);
    model.page = COFFEE_DATE; model.date_for_clock = true; model.date_field = 5; model.draft_date = 20991231;
    render("date-editor",&model,&info,0); model.editing = true; render(NULL,&model,&info,0);
    model.page = COFFEE_NETWORK; info.network.status = COFFEE_NET_AP_READY; info.network.provisioning = true;
    snprintf(info.network.ap_ssid,sizeof(info.network.ap_ssid),"CoffeeNotes-1234"); snprintf(info.network.ap_password,sizeof(info.network.ap_password),"12345678");
    render("network",&model,&info,0);
    info.network.provisioning = false; info.network.clock_valid = true; info.network.status = COFFEE_NET_SYNCED; render("network-synced",&model,&info,0);
    model.page = COFFEE_DELETE; model.cursor = 20261006; model.day_focus = 6; model.focus = 1; render("delete-confirm",&model,&info,0);
    model.page = COFFEE_FORGET; model.focus = 0; render("forget-confirm",&model,&info,0);
    model.notice = COFFEE_NOTICE_FULL; render("capacity-full",&model,&info,0);
    info.storage_ready = false; render("storage-failure",&model,&info,0);
    info.input_ready = false; render("input-failure",&model,&info,0);
    info.input_ready = info.storage_ready = true; info.battery = -1; model.notice = COFFEE_NOTICE_NONE;
    const uint32_t sample_dates[] = {20261001,20261002,20261002,20261003,20261003,20261003,
                                   20261005,20261006,20261006,20261006,20261007,20261007,20260930};
    data.count = sizeof(sample_dates) / sizeof(sample_dates[0]);
    for (unsigned i = 0; i < data.count; ++i)
        data.entries[i] = (coffee_entry_t){sample_dates[i],(uint16_t)(540 + i * 20),0,(uint8_t)(i % COFFEE_TYPE_COUNT),COFFEE_NO_RECIPE,0};
    const coffee_page_t pages[] = {COFFEE_HOME,COFFEE_CALENDAR,COFFEE_DAY,COFFEE_RECORD,
        COFFEE_RECIPES,COFFEE_RECIPE,COFFEE_EDIT_RECIPE,COFFEE_TIMER,COFFEE_SETTINGS,
        COFFEE_DATE,COFFEE_NETWORK,COFFEE_DELETE,COFFEE_FORGET,COFFEE_THEMES};
    const char *periods[] = {"今日咖啡","本周咖啡","本月咖啡"};
    char render_file[64];
    for (unsigned theme = 0; theme < COFFEE_THEME_COUNT; ++theme) {
        data.theme = (uint8_t)theme; coffee_model_init(&model,&data);
        coffee_model_clock(&model,20261006,630,true); model.recipe = 0;
        model.draft_recipe = data.recipes[0]; model.draft_theme = (uint8_t)theme;
        for (unsigned period = 0; period < COFFEE_PERIOD_COUNT; ++period) {
            data.home_period = (uint8_t)period; model.page = COFFEE_HOME; model.focus = 4;
            snprintf(render_file,sizeof(render_file),"theme-%u-home-%u",theme,period);
            render(render_file,&model,&info,0);
            assert(find_label(lv_screen_active(), periods[period]));
        }
        data.home_period = COFFEE_TODAY; model.page = COFFEE_CALENDAR; model.cursor = 20261006;
        snprintf(render_file,sizeof(render_file),"theme-%u-calendar",theme);
        render(render_file,&model,&info,0); audit_heatmap();
        for (unsigned page = 0; page < sizeof(pages) / sizeof(pages[0]); ++page) {
            model.page = pages[page]; model.focus = 0; model.day_focus = 0;
            model.draft_clock_valid = true; model.type = COFFEE_HAND;
            model.draft_date = 20261006; model.draft_minute = 630;
            model.timer_active = true; model.timer_paused = false; model.timer_accum = 0; model.timer_anchor = 0;
            if (model.page == COFFEE_THEMES) snprintf(render_file,sizeof(render_file),"theme-%u-picker",theme);
            else if (model.page == COFFEE_RECORD) snprintf(render_file,sizeof(render_file),"theme-%u-record",theme);
            else if (model.page == COFFEE_TIMER) snprintf(render_file,sizeof(render_file),"theme-%u-timer",theme);
            else render_file[0] = 0;
            render(render_file[0] ? render_file : NULL,&model,&info,70000);
        }
    }
    data.theme = 0; coffee_model_init(&model,&data); model.page = COFFEE_HOME;
    render(NULL,&model,&info,0);
    lv_color_t durable_background = lv_obj_get_style_bg_color(lv_screen_active(), LV_PART_MAIN);
    model.page = COFFEE_THEMES; model.draft_theme = 3;
    render(NULL,&model,&info,0);
    assert(!lv_color_eq(durable_background, lv_obj_get_style_bg_color(lv_screen_active(), LV_PART_MAIN)));
    coffee_model_key(&model,COFFEE_BACK,0); render(NULL,&model,&info,0);
    assert(model.page == COFFEE_SETTINGS && data.theme == 0);
    assert(lv_color_eq(durable_background, lv_obj_get_style_bg_color(lv_screen_active(), LV_PART_MAIN)));
    data.count = COFFEE_CAPACITY;
    for (unsigned i = 0; i < data.count; ++i) data.entries[i] = (coffee_entry_t){20261001 + i % 31, (uint16_t)(i % 1440),0,COFFEE_LATTE,COFFEE_NO_RECIPE,0};
    model.page = COFFEE_CALENDAR; model.cursor = 20261006; render("calendar-full-month",&model,&info,0);
    for (unsigned i = 0; i < data.count; ++i) data.entries[i].date = 20261006;
    model.page = COFFEE_HOME; render("home-512-cups",&model,&info,0);
    coffee_model_init(&model,&data);
    lv_mem_monitor_t before,after;
    model.page = COFFEE_HOME; render(NULL,&model,&info,0);
    lv_mem_monitor(&before);
    for (unsigned i = 0; i < 1200; ++i) {
        data.theme = (uint8_t)(i % COFFEE_THEME_COUNT); data.home_period = (uint8_t)(i % COFFEE_PERIOD_COUNT);
        model.page = i % 3 == 0 ? COFFEE_CALENDAR : i % 3 == 1 ? COFFEE_HOME : COFFEE_THEMES;
        model.draft_theme = data.theme; model.cursor = coffee_date_month(20261006,(int)i % 12); model.focus = i % 5;
        render(NULL,&model,&info,0);
        if (i == 99) { model.page = COFFEE_HOME; data.theme = data.home_period = 0; model.focus = 0; render(NULL,&model,&info,0); lv_mem_monitor(&before); }
    }
    data.theme = data.home_period = 0; model.focus = 0;
    model.page = COFFEE_HOME; render(NULL,&model,&info,0); lv_mem_monitor(&after);
    assert(after.free_size >= before.free_size && after.free_biggest_size >= 2048);
    printf("LVGL 14 pages, 4 palettes, 3 periods, intensity/selection, actual fonts, bounds and 1200 transitions: PASS\n");
    printf("LVGL 40 KB pool: free=%lu largest=%lu peak=%lu bytes\n",(unsigned long)after.free_size,(unsigned long)after.free_biggest_size,(unsigned long)after.max_used);
    lv_deinit(); return 0;
}
