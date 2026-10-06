#include "coffee_ui.h"
#include "lvgl.h"
#include "coffee_glyphs.h"
#include <stdio.h>
#include <string.h>

LV_FONT_DECLARE(coffee_font_14);
LV_FONT_DECLARE(coffee_font_16);
LV_FONT_DECLARE(coffee_font_20);

typedef struct {
    uint32_t paper, ink, muted, accent, card, on_accent, selected, on_selected;
    uint32_t heat[4], heat_text[4];
} palette_t;
static const palette_t palettes[COFFEE_THEME_COUNT] = {
    {0xF5EFE4,0x392E29,0x89766A,0xA24F32,0xE8DDCD,0xFFF7EA,0x392E29,0xF5EFE4,
     {0xEEE7DD,0xDBC3A6,0xBB906B,0x805436},{0x392E29,0x392E29,0x392E29,0xFFF7EA}},
    {0xEDF2E7,0x233C30,0x607465,0x437353,0xD9E4D4,0xF8FBEF,0x2A4636,0xF8FBEF,
     {0xE1E8DA,0xBDCFB0,0x84A77B,0x496F49},{0x233C30,0x233C30,0x233C30,0xF8FBEF}},
    {0xEEF4F8,0x263848,0x62788A,0x316F9A,0xDCE8F1,0xF7FBFF,0x2E4F67,0xF7FBFF,
     {0xE1EAF0,0xBCD3E3,0x7CA8C7,0x386788},{0x263848,0x263848,0x263848,0xF7FBFF}},
    {0x16212C,0xEAF0F5,0xA5B6C5,0x8AB9D5,0x243746,0x172D3A,0x365064,0xF2F7FB,
     {0x243746,0x426780,0x6F98B4,0xA4C7DC},{0xD8E3ED,0xF2F7FB,0x16212C,0x16212C}}
};
static const palette_t *palette = &palettes[0];
#define PAPER (palette->paper)
#define INK (palette->ink)
#define MUTED (palette->muted)
#define ACCENT (palette->accent)
#define CREAM (palette->card)
#define ON_ACCENT (palette->on_accent)

static lv_obj_t *screen, *body, *brand_label, *battery_label, *foot_label;
static const char *const type_names[] = {"手冲", "意式", "美式", "拿铁", "冷萃", "其他"};
static const char *const recipe_names[] = {"清爽手冲", "醇厚手冲", "冰手冲"};
static const char *const stage_names[] = {"闷蒸", "第一段注水", "第二段注水", "收尾与滴滤"};
static const char *const stage_advice[] = {"轻柔润湿全部咖啡粉", "小水流绕圈 均匀注水", "保持水位 避免冲到滤纸", "补足目标水量 静置滴滤"};
static const char *const status_names[] = {"网络未启动", "尚未配置网络", "正在连接网络", "已联网 等待校时", "北京时间已同步", "连接失败 可重新配置", "热点已就绪", "正在验证网络", "网络配置已保存", "网络存储失败"};
static const char *const period_names[] = {"今日咖啡", "本周咖啡", "本月咖啡"};
static const char *const theme_names[] = {"咖啡棕", "抹茶绿", "海盐蓝", "深夜黑"};

static lv_obj_t *box(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color, int radius)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y); lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(o, radius, 0);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}
static lv_obj_t *text(lv_obj_t *parent, int x, int y, int w, const char *s, const lv_font_t *font, uint32_t color)
{
    lv_obj_t *o = lv_label_create(parent);
    lv_obj_set_pos(o, x, y); lv_obj_set_width(o, w);
    lv_obj_set_style_text_font(o, font, 0); lv_obj_set_style_text_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_text_line_space(o, 2, 0);
    lv_label_set_text(o, s);
    return o;
}
static void row(int y, int h, const char *s, const char *value, bool selected)
{
    lv_obj_t *o = box(body, 14, y, 212, h, selected ? palette->selected : CREAM, 7);
    text(o, 10, (h - 18) / 2 - 2, value ? 104 : 192, s, &coffee_font_16, selected ? palette->on_selected : INK);
    if (value) {
        lv_obj_t *v = text(o, 118, (h - 18) / 2 - 2, 84, value, &coffee_font_16, selected ? palette->on_selected : MUTED);
        lv_obj_set_style_text_align(v, LV_TEXT_ALIGN_RIGHT, 0);
    }
}
static void title(const char *s, const char *subtitle)
{
    text(body, 16, 40, 210, s, &coffee_font_20, INK);
    if (subtitle) text(body, 16, 67, 210, subtitle, &coffee_font_14, MUTED);
}
static void footer(const char *s) { lv_label_set_text(foot_label, s); }
static void date_text(char *out, size_t n, uint32_t date)
{
    snprintf(out, n, "%04lu.%02lu.%02lu", (unsigned long)(date / 10000), (unsigned long)(date / 100 % 100), (unsigned long)(date % 100));
}
static void home(const coffee_model_t *m)
{
    char buf[80];
    title("咖啡手记", m->clock_valid ? "把每一杯 留在日历里" : "日期待确认");
    unsigned period = m->data->home_period < COFFEE_PERIOD_COUNT ? m->data->home_period : COFFEE_TODAY;
    lv_obj_t *hero = box(body, 14, 92, 212, 65, ACCENT, 10);
    if (m->focus == 4) {
        lv_obj_set_style_outline_color(hero, lv_color_hex(INK), 0);
        lv_obj_set_style_outline_width(hero, 2, 0);
        lv_obj_set_style_outline_pad(hero, 2, 0);
    }
    if (period == COFFEE_WEEK) {
        uint32_t start = coffee_week_start(m->now_date), end = coffee_week_end(m->now_date);
        snprintf(buf, sizeof(buf), "%02lu/%02lu - %02lu/%02lu", (unsigned long)(start / 100 % 100), (unsigned long)(start % 100), (unsigned long)(end / 100 % 100), (unsigned long)(end % 100));
    } else if (period == COFFEE_MONTH) {
        snprintf(buf, sizeof(buf), "%04lu/%02lu", (unsigned long)(m->now_date / 10000), (unsigned long)(m->now_date / 100 % 100));
    } else snprintf(buf, sizeof(buf), "%02lu / %02lu", (unsigned long)(m->now_date / 100 % 100), (unsigned long)(m->now_date % 100));
    text(hero, 12, period == COFFEE_WEEK ? 16 : 8, 123, buf, period == COFFEE_WEEK ? &lv_font_montserrat_14 : &lv_font_montserrat_28, ON_ACCENT);
    text(hero, 12, 42, 126, period_names[period], &coffee_font_14, ON_ACCENT);
    snprintf(buf, sizeof(buf), "%u", coffee_period_count(m->data, m->now_date, (coffee_period_t)period));
    lv_obj_t *count = text(hero, 134, 8, 60, buf, &lv_font_montserrat_28, ON_ACCENT);
    lv_obj_set_style_text_align(count, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_t *cups = text(hero, 152, 42, 40, "杯", &coffee_font_14, ON_ACCENT);
    lv_obj_set_style_text_align(cups, LV_TEXT_ALIGN_RIGHT, 0);
    const char *names[] = {"记录一杯", "咖啡日历", "冲煮手册", "设置"};
    for (unsigned i = 0; i < 4; ++i) row(167 + (int)i * 27, 24, names[i], NULL, m->focus == i);
    footer(m->focus == 4 ? "确定换范围" : "上下选择  确定打开");
}
static void calendar(const coffee_model_t *m)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "%04lu / %02lu  ·  %u杯", (unsigned long)(m->cursor / 10000), (unsigned long)(m->cursor / 100 % 100), coffee_month_count(m->data, m->cursor));
    title("咖啡日历", buf);
    const char *week[] = {"一", "二", "三", "四", "五", "六", "日"};
    for (int col = 0; col < 7; ++col) {
        lv_obj_t *l = text(body, 15 + col * 30, 92, 28, week[col], &coffee_font_14, MUTED);
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    }
    uint32_t first = m->cursor / 100 * 100 + 1;
    unsigned start = coffee_weekday(first), days = coffee_month_days(first / 10000, first / 100 % 100);
    for (unsigned day = 1; day <= days; ++day) {
        unsigned cell = start + day - 1;
        uint32_t date = first + day - 1;
        bool selected = date == m->cursor;
        unsigned cups = coffee_day_count(m->data, date), level = cups > 3 ? 3 : cups;
        snprintf(buf, sizeof(buf), "%u", day);
        lv_obj_t *l = text(body, 15 + (int)(cell % 7) * 30, 113 + (int)(cell / 7) * 23, 28, buf, &coffee_font_14, palette->heat_text[level]);
        lv_obj_set_height(l, 22);
        lv_obj_set_style_bg_color(l, lv_color_hex(palette->heat[level]), 0);
        lv_obj_set_style_bg_opa(l, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(l, 5, 0);
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
        if (selected) {
            lv_obj_set_style_outline_color(l, lv_color_hex(INK), 0);
            lv_obj_set_style_outline_width(l, 2, 0);
        }
    }
    snprintf(buf, sizeof(buf), "%02lu日 %u杯", (unsigned long)(m->cursor % 100), coffee_day_count(m->data, m->cursor));
    text(body, 16, 256, 85, buf, &coffee_font_14, MUTED);
    const char *levels[] = {"0","1","2","3+"};
    for (unsigned i = 0; i < 4; ++i) {
        lv_obj_t *l = text(body, 106 + (int)i * 29, 256, 24, levels[i], &lv_font_montserrat_14, palette->heat_text[i]);
        lv_obj_set_height(l, 21); lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_bg_color(l, lv_color_hex(palette->heat[i]), 0);
        lv_obj_set_style_bg_opa(l, LV_OPA_COVER, 0); lv_obj_set_style_radius(l, 4, 0);
    }
    footer("上下选日  长按换月\n确定查看  长按返回");
}
static void day(const coffee_model_t *m)
{
    char buf[80], date[24]; date_text(date, sizeof(date), m->cursor);
    title("这一天的咖啡", date);
    unsigned count = coffee_day_count(m->data, m->cursor), total = count + 1;
    unsigned start = m->day_focus >= 5 ? m->day_focus - 4 : 0;
    for (unsigned slot = 0; slot < 5 && start + slot < total; ++slot) {
        unsigned index = start + slot;
        if (index == count) row(95 + (int)slot * 32, 28, "+ 记录一杯", NULL, m->day_focus == index);
        else {
            int ei = coffee_day_entry(m->data, m->cursor, index); const coffee_entry_t *e = &m->data->entries[ei];
            snprintf(buf, sizeof(buf), "%02u:%02u  %s%s", e->minute / 60, e->minute % 60, type_names[e->type], e->manual_time ? " *" : "");
            row(95 + (int)slot * 32, 28, buf, NULL, m->day_focus == index);
        }
    }
    snprintf(buf, sizeof(buf), "%u杯  ·  * 人工确认日期", count);
    text(body, 16, 261, 210, buf, &coffee_font_14, MUTED);
    footer("确定查看  长按返回");
}
static void record(const coffee_model_t *m)
{
    char buf[80], date[24]; date_text(date, sizeof(date), m->draft_date);
    title("记录一杯", NULL);
    lv_obj_t *card = box(body, 14, 98, 212, 123, CREAM, 10);
    text(card, 15, 12, 180, "这杯是", &coffee_font_14, MUTED);
    text(card, 15, 38, 180, type_names[m->type], &coffee_font_20, ACCENT);
    snprintf(buf, sizeof(buf), "%s  %02u:%02u", date, m->draft_minute / 60, m->draft_minute % 60);
    text(card, 15, 78, 190, buf, &coffee_font_14, INK);
    if (!m->draft_clock_valid && !m->manually_set)
        text(card, 15, 100, 190, "保存前确认日期", &coffee_font_14, MUTED);
    if (m->record_seconds) {
        snprintf(buf, sizeof(buf), "冲煮 %02u:%02u  ·  %s", m->record_seconds / 60, m->record_seconds % 60, m->recipe < COFFEE_RECIPE_COUNT ? recipe_names[m->recipe] : "手冲");
        text(body, 16, 236, 210, buf, &coffee_font_14, MUTED);
    }
    footer("上下选类型  确定保存\n长按上改日期  长按取消");
}
static void recipes(const coffee_model_t *m)
{
    title("冲煮手册", "参考起点 按口味调整");
    char buf[80];
    for (unsigned i = 0; i < COFFEE_RECIPE_COUNT; ++i) {
        lv_obj_t *card = box(body, 14, 94 + (int)i * 57, 212, 50, m->focus == i ? palette->selected : CREAM, 8);
        text(card, 12, 5, 188, recipe_names[i], &coffee_font_16, m->focus == i ? palette->on_selected : INK);
        snprintf(buf, sizeof(buf), "%ug / %uml / %uC / %u:%02u", m->data->recipes[i].grams, m->data->recipes[i].water_ml, m->data->recipes[i].degrees, m->data->recipes[i].seconds / 60, m->data->recipes[i].seconds % 60);
        text(card, 12, 28, 190, buf, &coffee_font_14, m->focus == i ? palette->on_selected : MUTED);
    }
    text(body, 16, 265, 210, "冰手冲另加冰块100g", &coffee_font_14, MUTED);
    footer("确定查看  长按返回");
}
static void recipe(const coffee_model_t *m)
{
    const coffee_recipe_t *r = &m->data->recipes[m->recipe]; char buf[80];
    title(recipe_names[m->recipe], "参数与分段注水参考");
    snprintf(buf, sizeof(buf), "%ug  :  %uml  |  %uC", r->grams, r->water_ml, r->degrees);
    text(body, 16, 93, 210, buf, &coffee_font_20, ACCENT);
    const unsigned at[] = {0,20,45,70};
    for (unsigned i = 0; i < 4; ++i) {
        unsigned s = r->seconds * at[i] / 100;
        snprintf(buf, sizeof(buf), "%u:%02u  %s  %uml", s / 60, s % 60, i == 0 ? "闷蒸" : i == 3 ? "收尾" : "注水", coffee_stage_water(r, i));
        text(body, 16, 131 + (int)i * 23, 210, buf, &coffee_font_14, INK);
    }
    row(229, 24, "开始计时", NULL, m->focus == 0);
    row(258, 24, "调整参数", NULL, m->focus == 1);
    footer("确定打开  长按返回");
}
static void edit_recipe(const coffee_model_t *m)
{
    title("我的冲煮参数", m->editing ? "上/下修改 确定完成此项" : "选择参数 确定进入修改");
    const char *names[] = {"咖啡粉", "热水量", "水温", "目标时间", "保存参数"};
    for (unsigned i = 0; i < 5; ++i) {
        char buf[32];
        if (i == 0) snprintf(buf, sizeof(buf), "%ug", m->draft_recipe.grams);
        else if (i == 1) snprintf(buf, sizeof(buf), "%uml", m->draft_recipe.water_ml);
        else if (i == 2) snprintf(buf, sizeof(buf), "%uC", m->draft_recipe.degrees);
        else snprintf(buf, sizeof(buf), "%u:%02u", m->draft_recipe.seconds / 60, m->draft_recipe.seconds % 60);
        row(95 + (int)i * 34, 29, names[i], i == 4 ? NULL : buf, m->focus == i);
    }
    footer(m->editing ? "上下调整  确定完成" : m->focus == 4 ? "确定保存  长按取消" : "确定修改  长按取消");
}
static void timer(const coffee_model_t *m, uint64_t now_ms)
{
    const coffee_recipe_t *r = &m->data->recipes[m->recipe]; unsigned seconds = coffee_timer_seconds(m, now_ms), stage = coffee_timer_stage(m, now_ms);
    char buf[80]; title(recipe_names[m->recipe], m->timer_paused ? "已暂停" : seconds >= r->seconds ? "目标时间已到 可完成记录" : "正在冲煮");
    snprintf(buf, sizeof(buf), "%02u:%02u", seconds / 60, seconds % 60);
    lv_obj_t *digits = text(body, 16, 100, 208, buf, &lv_font_montserrat_40, ACCENT);
    lv_obj_set_style_text_align(digits, LV_TEXT_ALIGN_CENTER, 0);
    snprintf(buf, sizeof(buf), "目标 %u:%02u  ·  %ug / %uml", r->seconds / 60, r->seconds % 60, r->grams, r->water_ml);
    text(body, 16, 153, 210, buf, &coffee_font_14, MUTED);
    box(body, 16, 178, 208, 4, CREAM, 2);
    unsigned width = seconds >= r->seconds ? 208 : seconds * 208 / r->seconds;
    if (width) box(body, 16, 178, (int)width, 4, ACCENT, 2);
    lv_obj_t *card = box(body, 14, 197, 212, 75, CREAM, 10);
    text(card, 12, 7, 188, stage_names[stage], &coffee_font_20, INK);
    snprintf(buf, sizeof(buf), "累计注水至 %uml", coffee_stage_water(r, stage));
    text(card, 12, 34, 188, buf, &coffee_font_16, ACCENT);
    text(card, 12, 56, 192, stage_advice[stage], &coffee_font_14, MUTED);
    footer(m->timer_paused ? "确定继续  上键完成\n长按取消" : "确定暂停  上键完成\n长按取消");
}
static void settings(const coffee_model_t *m, const coffee_ui_info_t *info)
{
    title("设置", m->clock_valid ? "日期与网络  配色风格" : "日期待确认");
    const char *names[] = {"配置 Wi-Fi", "手动调整日期", "配色风格", "忘记网络"};
    for (unsigned i = 0; i < 4; ++i) row(94 + (int)i * 36, 30, names[i], i == 2 ? theme_names[m->data->theme] : NULL, m->focus == i);
    unsigned status = info->network.status;
    text(body, 16, 241, 210, status < sizeof(status_names) / sizeof(status_names[0]) ? status_names[status] : "网络状态未知", &coffee_font_14, MUTED);
    footer("确定打开  长按返回");
}
static void themes(const coffee_model_t *m)
{
    title("配色风格", "上下预览  确定保存");
    for (unsigned i = 0; i < COFFEE_THEME_COUNT; ++i)
        row(99 + (int)i * 40, 34, theme_names[i], m->data->theme == i ? "当前" : NULL, m->draft_theme == i);
    footer("确定保存  长按取消");
}
static void date_editor(const coffee_model_t *m)
{
    title(m->date_for_clock ? "手动设置日期" : "确认记录日期", m->editing ? "上/下修改 确定完成此项" : "选择项目 确定修改");
    const char *names[] = {"年", "月", "日", "时", "分", "确认日期"};
    unsigned values[] = {m->draft_date / 10000, m->draft_date / 100 % 100, m->draft_date % 100, m->draft_minute / 60, m->draft_minute % 60};
    for (unsigned i = 0; i < 6; ++i) { char buf[24]; snprintf(buf, sizeof(buf), "%02u", i < 5 ? values[i] : 0); row(93 + (int)i * 29, 25, names[i], i == 5 ? NULL : buf, m->date_field == i); }
    footer(m->editing ? "上下调整  确定完成" : m->date_field == 5 ? "确定日期  长按取消" : "确定修改  长按取消");
}
static void network(const coffee_ui_info_t *info)
{
    unsigned status = info->network.status;
    title("连接咖啡手记", status < sizeof(status_names) / sizeof(status_names[0]) ? status_names[status] : "网络状态未知");
    if (info->network.provisioning && info->network.ap_ssid[0]) {
        text(body, 16, 98, 210, "1 手机连接此热点", &coffee_font_16, INK);
        text(body, 16, 126, 210, info->network.ap_ssid, &coffee_font_16, ACCENT);
        char buf[48]; snprintf(buf, sizeof(buf), "密码 %s", info->network.ap_password);
        text(body, 16, 150, 210, buf, &coffee_font_16, INK);
        text(body, 16, 190, 210, "2 浏览器打开", &coffee_font_16, INK);
        text(body, 16, 218, 210, "192.168.4.1", &coffee_font_20, ACCENT);
        text(body, 16, 251, 210, "热点5分钟后自动关闭", &coffee_font_14, MUTED);
    } else {
        text(body, 16, 109, 208, info->network.clock_valid ? "已同步北京时间" : "联网后同步北京时间", &coffee_font_16, INK);
        text(body, 16, 156, 208, "确定开启新的配网热点", &coffee_font_16, ACCENT);
        text(body, 16, 193, 208, "未校时也能手动确认日期记录", &coffee_font_14, MUTED);
    }
    footer("确定重试  长按返回");
}
static void deletion(const coffee_model_t *m)
{
    int index = coffee_day_entry(m->data, m->cursor, m->day_focus);
    title("这杯咖啡", "删除前请核对记录");
    if (index >= 0) {
        const coffee_entry_t *e = &m->data->entries[index]; char buf[80], date[24]; date_text(date, sizeof(date), e->date);
        text(body, 16, 102, 210, type_names[e->type], &coffee_font_20, ACCENT);
        snprintf(buf, sizeof(buf), "%s  %02u:%02u", date, e->minute / 60, e->minute % 60); text(body, 16, 139, 210, buf, &coffee_font_14, INK);
        snprintf(buf, sizeof(buf), "冲煮时长 %u:%02u", e->duration / 60, e->duration % 60); text(body, 16, 169, 210, e->duration ? buf : "快速记录", &coffee_font_16, MUTED);
    }
    row(222, 28, "保留记录", NULL, m->focus == 0);
    row(258, 28, "确认删除", NULL, m->focus == 1);
    footer("确定选择  长按返回");
}
static void forget(const coffee_model_t *m)
{
    title("忘记网络", "仅清除已保存的Wi-Fi凭据");
    text(body, 16, 110, 208, "咖啡记录与冲煮参数会保留", &coffee_font_16, INK);
    text(body, 16, 151, 208, "以后可重新配网", &coffee_font_16, MUTED);
    row(221, 28, "取消", NULL, m->focus == 0); row(257, 28, "确认忘记网络", NULL, m->focus == 1);
    footer("确定选择  长按取消");
}
bool coffee_ui_verify_fonts(void)
{
    const lv_font_t *fonts[] = {&coffee_font_14,&coffee_font_16,&coffee_font_20};
    for (unsigned f = 0; f < 3; ++f) for (unsigned i = 0; i < sizeof(coffee_glyphs) / sizeof(coffee_glyphs[0]); ++i) {
        lv_font_glyph_dsc_t glyph = {0};
        if (!lv_font_get_glyph_dsc(fonts[f], &glyph, coffee_glyphs[i], 0) || glyph.is_placeholder) return false;
    }
    return true;
}
bool coffee_ui_create(void)
{
    screen = lv_obj_create(NULL);
    if (!screen) return false;
    lv_obj_remove_style_all(screen);
    lv_obj_set_style_bg_color(screen, lv_color_hex(PAPER), 0); lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    body = box(screen, 0, 0, 240, 320, PAPER, 0);
    brand_label = text(screen, 18, 12, 160, "COFFEE / NOTES", &lv_font_montserrat_14, MUTED);
    battery_label = text(screen, 183, 12, 39, "--", &lv_font_montserrat_14, MUTED);
    lv_obj_set_style_text_align(battery_label, LV_TEXT_ALIGN_RIGHT, 0);
    foot_label = text(screen, 18, 284, 204, "", &coffee_font_14, MUTED);
    lv_obj_set_style_text_line_space(foot_label, -2, 0);
    lv_screen_load(screen); return true;
}
void coffee_ui_render(const coffee_model_t *m, const coffee_ui_info_t *info, uint64_t now_ms)
{
    unsigned theme = m->page == COFFEE_THEMES ? m->draft_theme : m->data->theme;
    palette = &palettes[theme < COFFEE_THEME_COUNT ? theme : 0];
    lv_obj_set_style_bg_color(screen, lv_color_hex(PAPER), 0);
    lv_obj_set_style_bg_color(body, lv_color_hex(PAPER), 0);
    lv_obj_set_style_text_color(brand_label, lv_color_hex(MUTED), 0);
    lv_obj_set_style_text_color(battery_label, lv_color_hex(MUTED), 0);
    lv_obj_set_style_text_color(foot_label, lv_color_hex(MUTED), 0);
    lv_obj_clean(body);
    switch (m->page) {
    case COFFEE_HOME: home(m); break;
    case COFFEE_CALENDAR: calendar(m); break;
    case COFFEE_DAY: day(m); break;
    case COFFEE_RECORD: record(m); break;
    case COFFEE_RECIPES: recipes(m); break;
    case COFFEE_RECIPE: recipe(m); break;
    case COFFEE_EDIT_RECIPE: edit_recipe(m); break;
    case COFFEE_TIMER: timer(m, now_ms); break;
    case COFFEE_SETTINGS: settings(m, info); break;
    case COFFEE_DATE: date_editor(m); break;
    case COFFEE_NETWORK: network(info); break;
    case COFFEE_DELETE: deletion(m); break;
    case COFFEE_FORGET: forget(m); break;
    case COFFEE_THEMES: themes(m); break;
    }
    char buf[16];
    if (info->battery >= 0 && info->battery <= 100) snprintf(buf, sizeof(buf), "%d%%", info->battery);
    else snprintf(buf, sizeof(buf), "--");
    lv_label_set_text(battery_label, buf);
    /* Failure overlays replace hints instead of falsely reporting durable data. */
    if (!info->input_ready) footer("按键异常 请重启");
    else if (!info->storage_ready) footer("存储异常 请重启");
    else if (m->notice == COFFEE_NOTICE_SAVED) footer("已保存");
    else if (m->notice == COFFEE_NOTICE_FAILED) footer("操作失败 请重试");
    else if (m->notice == COFFEE_NOTICE_FULL) footer("记录已满 请先删除");
}
