#include "Translator_UI.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Wireless.h"

#define BG       0x07111F
#define CARD     0x122238
#define ACCENT   0x5B8CFF
#define MINT     0x34D399
#define TEXT     0xF2F6FF
#define MUTED    0x91A3BC

static lv_obj_t *s_root;
static lv_obj_t *s_body;
static lv_obj_t *s_status;
static lv_obj_t *s_password;
static lv_obj_t *s_keyboard;
static lv_obj_t *s_source;
static lv_obj_t *s_translation;
static lv_obj_t *s_source_lang_btn;
static lv_obj_t *s_target_lang_btn;
static lv_obj_t *s_source_lang_label;
static lv_obj_t *s_target_lang_label;
static lv_obj_t *s_language_panel;
static bool s_choosing_source;
static lv_obj_t *s_wave[5];
static lv_timer_t *s_poll;
static wifi_prov_state_t s_last_state = -1;
static char s_selected_ssid[33];
static const char *s_languages[] = {
    "English", "Chinese", "Spanish", "Japanese", "French", "German", "Korean"
};
static uint8_t s_source_language = 0;
static uint8_t s_target_language = 1;


static void style_card(lv_obj_t *obj)
{
    lv_obj_set_style_bg_color(obj, lv_color_hex(CARD), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, 18, 0);
    lv_obj_set_style_pad_all(obj, 14, 0);
}

static lv_obj_t *label(lv_obj_t *parent, const char *text, lv_color_t color)
{
    lv_obj_t *obj = lv_label_create(parent);
    lv_label_set_text(obj, text);
    lv_obj_set_style_text_color(obj, color, 0);
    return obj;
}

static void show_translate(void);
static void show_setup(void);

static void close_language_panel(void)
{
    if (s_language_panel) {
        lv_obj_del(s_language_panel);
        s_language_panel = NULL;
    }
}

static void language_item_event(lv_event_t *e)
{
    uint8_t index = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if (s_choosing_source) s_source_language = index;
    else s_target_language = index;
    lv_label_set_text(s_source_lang_label, s_languages[s_source_language]);
    lv_label_set_text(s_target_lang_label, s_languages[s_target_language]);
    close_language_panel();
}

static void language_button_event(lv_event_t *e)
{
    s_choosing_source = lv_event_get_target(e) == s_source_lang_btn;
    close_language_panel();

    s_language_panel = lv_obj_create(s_root);
    lv_obj_set_size(s_language_panel, 210, 220);
    lv_obj_center(s_language_panel);
    style_card(s_language_panel);
    lv_obj_set_style_bg_color(s_language_panel, lv_color_hex(0x0D1B2D), 0);
    lv_obj_set_style_border_color(s_language_panel, lv_color_hex(ACCENT), 0);
    lv_obj_set_style_border_width(s_language_panel, 2, 0);
    lv_obj_set_flex_flow(s_language_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(s_language_panel, 10, 0);
    lv_obj_set_style_pad_row(s_language_panel, 5, 0);
    label(s_language_panel, s_choosing_source ? "Translate from" : "Translate to", lv_color_hex(MINT));

    for (uint8_t i = 0; i < sizeof(s_languages) / sizeof(s_languages[0]); ++i) {
        lv_obj_t *item = lv_btn_create(s_language_panel);
        lv_obj_set_size(item, lv_pct(100), 29);
        lv_obj_set_style_bg_color(item, lv_color_hex(0x1B3154), 0);
        lv_obj_set_style_radius(item, 12, 0);
        label(item, s_languages[i], lv_color_hex(TEXT));
        lv_obj_add_event_cb(item, language_item_event, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
    }
    lv_obj_move_foreground(s_language_panel);
}

static void swap_language_event(lv_event_t *e)
{
    uint8_t tmp = s_source_language;
    s_source_language = s_target_language;
    s_target_language = tmp;
    lv_label_set_text(s_source_lang_label, s_languages[s_source_language]);
    lv_label_set_text(s_target_lang_label, s_languages[s_target_language]);
}

static void wifi_back_event(lv_event_t *e)
{
    close_language_panel();
    lv_label_set_text(s_status, "Select a network");
    WIFI_StartScan();
    show_setup();
}

static void keyboard_event(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY) {
        WIFI_Connect(s_selected_ssid, lv_textarea_get_text(s_password));
        lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_password, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_status, "Connecting...");
    } else if (code == LV_EVENT_CANCEL) {
        lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_password, LV_OBJ_FLAG_HIDDEN);
    }
}

static void ap_event(lv_event_t *e)
{
    const char *ssid = lv_event_get_user_data(e);
    strlcpy(s_selected_ssid, ssid, sizeof(s_selected_ssid));
    lv_textarea_set_text(s_password, "");
    lv_textarea_set_placeholder_text(s_password, s_selected_ssid);
    lv_obj_clear_flag(s_password, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_keyboard_set_textarea(s_keyboard, s_password);
}

static void scan_event(lv_event_t *e)
{
    WIFI_StartScan();
    lv_label_set_text(s_status, "Scanning nearby networks...");
}

static void demo_event(lv_event_t *e) { show_translate(); }

static void show_setup(void)
{
    lv_obj_clean(s_body);
    lv_obj_set_flex_flow(s_body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_body, 5, 0);

    lv_obj_t *title = label(s_body, "Let's get online", lv_color_hex(TEXT));
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    label(s_body, "Choose Wi-Fi to enable cloud translation", lv_color_hex(MUTED));

    wifi_prov_ap_t aps[WIFI_PROV_MAX_AP];
    size_t count = WIFI_GetScanResults(aps, WIFI_PROV_MAX_AP);
    for (size_t i = 0; i < count && i < 5; ++i) {
        lv_obj_t *btn = lv_btn_create(s_body);
        lv_obj_set_size(btn, lv_pct(100), 44);
        style_card(btn);
        lv_obj_set_style_bg_color(btn, lv_color_hex(i == 0 ? 0x1B3154 : CARD), 0);
        char row[56];
        snprintf(row, sizeof(row), "%s                 %s", aps[i].ssid,
                 aps[i].authmode == WIFI_AUTH_OPEN ? "Open" : LV_SYMBOL_WIFI);
        label(btn, row, lv_color_hex(TEXT));
        char *ssid_copy = lv_mem_alloc(strlen(aps[i].ssid) + 1);
        if (ssid_copy) { strcpy(ssid_copy, aps[i].ssid); lv_obj_add_event_cb(btn, ap_event, LV_EVENT_CLICKED, ssid_copy); }
    }

    lv_obj_t *actions = lv_obj_create(s_body);
    lv_obj_set_size(actions, lv_pct(100), 46);
    lv_obj_set_style_bg_opa(actions, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(actions, 0, 0);
    lv_obj_set_flex_flow(actions, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(actions, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *scan = lv_btn_create(actions);
    lv_obj_set_style_bg_color(scan, lv_color_hex(ACCENT), 0);
    label(scan, LV_SYMBOL_REFRESH " Scan", lv_color_hex(TEXT));
    lv_obj_add_event_cb(scan, scan_event, LV_EVENT_CLICKED, NULL);
    lv_obj_t *demo = lv_btn_create(actions);
    lv_obj_set_style_bg_color(demo, lv_color_hex(0x26364B), 0);
    label(demo, "Try UI", lv_color_hex(TEXT));
    lv_obj_add_event_cb(demo, demo_event, LV_EVENT_CLICKED, NULL);
}

static void mic_event(lv_event_t *e)
{
    static bool listening;
    listening = !listening;
    Translator_UI_SetListening(listening);
    if (listening) Translator_UI_SetTranscript("Listening...", "Speak naturally");
    else Translator_UI_SetTranscript("Good morning, how are you?", "Buenos dias, como estas?");
}

static void wave_anim(void *obj, int32_t value) { lv_obj_set_height(obj, value); }

static void show_translate(void)
{
    close_language_panel();
    lv_obj_clean(s_body);
    lv_obj_set_flex_flow(s_body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_body, 10, 0);
    lv_obj_t *head = lv_obj_create(s_body);
    lv_obj_set_size(head, lv_pct(100), 32);
    lv_obj_set_style_bg_opa(head, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(head, 0, 0);
    lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(head, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *wifi = lv_btn_create(head);
    lv_obj_set_size(wifi, 27, 27);
    lv_obj_set_style_radius(wifi, 14, 0);
    lv_obj_set_style_bg_color(wifi, lv_color_hex(0x26364B), 0);
    label(wifi, LV_SYMBOL_WIFI, lv_color_hex(TEXT));
    lv_obj_add_event_cb(wifi, wifi_back_event, LV_EVENT_CLICKED, NULL);

    s_source_lang_btn = lv_btn_create(head);
    lv_obj_set_size(s_source_lang_btn, 70, 27);
    lv_obj_set_style_radius(s_source_lang_btn, 11, 0);
    lv_obj_set_style_pad_all(s_source_lang_btn, 3, 0);
    lv_obj_set_style_bg_color(s_source_lang_btn, lv_color_hex(0x1B3154), 0);
    s_source_lang_label = label(s_source_lang_btn, s_languages[s_source_language], lv_color_hex(TEXT));
    lv_obj_center(s_source_lang_label);
    lv_obj_add_event_cb(s_source_lang_btn, language_button_event, LV_EVENT_CLICKED, NULL);

    lv_obj_t *swap = lv_btn_create(head);
    lv_obj_set_size(swap, 27, 27);
    lv_obj_set_style_radius(swap, 14, 0);
    lv_obj_set_style_bg_color(swap, lv_color_hex(0x26364B), 0);
    lv_obj_t *swap_icon = label(swap, LV_SYMBOL_SHUFFLE, lv_color_hex(ACCENT));
    lv_obj_center(swap_icon);
    lv_obj_add_event_cb(swap, swap_language_event, LV_EVENT_CLICKED, NULL);

    s_target_lang_btn = lv_btn_create(head);
    lv_obj_set_size(s_target_lang_btn, 70, 27);
    lv_obj_set_style_radius(s_target_lang_btn, 11, 0);
    lv_obj_set_style_pad_all(s_target_lang_btn, 3, 0);
    lv_obj_set_style_bg_color(s_target_lang_btn, lv_color_hex(0x1B3154), 0);
    s_target_lang_label = label(s_target_lang_btn, s_languages[s_target_language], lv_color_hex(TEXT));
    lv_obj_center(s_target_lang_label);
    lv_obj_add_event_cb(s_target_lang_btn, language_button_event, LV_EVENT_CLICKED, NULL);

    lv_obj_t *source_card = lv_obj_create(s_body);
    lv_obj_set_size(source_card, lv_pct(100), 62);
    lv_obj_set_style_pad_all(source_card, 10, 0);
    style_card(source_card);
    label(source_card, "YOU", lv_color_hex(ACCENT));
    s_source = label(source_card, "Tap the mic and start speaking", lv_color_hex(TEXT));
    lv_obj_align(s_source, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    lv_obj_t *translation_card = lv_obj_create(s_body);
    lv_obj_set_size(translation_card, lv_pct(100), 62);
    lv_obj_set_style_pad_all(translation_card, 10, 0);
    style_card(translation_card);
    label(translation_card, "TRANSLATION", lv_color_hex(MINT));
    s_translation = label(translation_card, "Your translation appears here", lv_color_hex(TEXT));
    lv_obj_align(s_translation, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    lv_obj_t *mic = lv_btn_create(s_body);
    lv_obj_set_size(mic, lv_pct(100), 42);
    lv_obj_set_style_bg_color(mic, lv_color_hex(ACCENT), 0);
    lv_obj_set_style_radius(mic, 24, 0);
    lv_obj_add_event_cb(mic, mic_event, LV_EVENT_CLICKED, NULL);
    lv_obj_t *waves = lv_obj_create(mic);
    lv_obj_set_size(waves, 92, 36);
    lv_obj_center(waves);
    lv_obj_set_style_bg_opa(waves, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(waves, 0, 0);
    lv_obj_set_flex_flow(waves, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(waves, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    for (int i = 0; i < 5; ++i) {
        s_wave[i] = lv_obj_create(waves);
        lv_obj_set_size(s_wave[i], 5, 10 + (i % 3) * 6);
        lv_obj_set_style_radius(s_wave[i], 3, 0);
        lv_obj_set_style_border_width(s_wave[i], 0, 0);
        lv_obj_set_style_bg_color(s_wave[i], lv_color_hex(TEXT), 0);
    }
}

void Translator_UI_SetListening(bool listening)
{
    for (int i = 0; i < 5; ++i) {
        lv_anim_del(s_wave[i], wave_anim);
        if (!listening) { lv_obj_set_height(s_wave[i], 10 + (i % 3) * 6); continue; }
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, s_wave[i]);
        lv_anim_set_exec_cb(&a, wave_anim);
        lv_anim_set_values(&a, 8, 30 - abs(2 - i) * 4);
        lv_anim_set_time(&a, 320 + i * 45);
        lv_anim_set_playback_time(&a, 320 + i * 45);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_start(&a);
    }
}

void Translator_UI_SetTranscript(const char *source, const char *translation)
{
    if (s_source) lv_label_set_text(s_source, source);
    if (s_translation) lv_label_set_text(s_translation, translation);
}

static void poll_wifi(lv_timer_t *timer)
{
    wifi_prov_state_t state = WIFI_GetState();
    if (state == s_last_state) return;
    s_last_state = state;
    if (state == WIFI_PROV_CONNECTED) {
        char text[64];
        snprintf(text, sizeof(text), LV_SYMBOL_WIFI " %s  %d dBm", WIFI_GetSSID(), WIFI_GetRSSI());
        lv_label_set_text(s_status, text);
        show_translate();
    } else if (state == WIFI_PROV_FAILED) {
        lv_label_set_text(s_status, "Connection failed - check password");
        show_setup();
    } else if (state == WIFI_PROV_IDLE) {
        lv_label_set_text(s_status, "Select a network");
        show_setup();
    }
}

void Translator_UI_Create(void)
{
    s_root = lv_scr_act();
    lv_obj_clean(s_root);
    lv_obj_set_style_bg_color(s_root, lv_color_hex(BG), 0);
    lv_obj_set_style_text_font(s_root, &lv_font_montserrat_14, 0);

    s_status = label(s_root, "Starting Wi-Fi...", lv_color_hex(MUTED));
    lv_obj_align(s_status, LV_ALIGN_TOP_MID, 0, 12);
    s_body = lv_obj_create(s_root);
    lv_obj_set_pos(s_body, 40, 45);
    lv_obj_set_size(s_body, 280, 270);
    lv_obj_set_style_bg_opa(s_body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_body, 0, 0);
    lv_obj_set_style_pad_all(s_body, 0, 0);
    show_setup();

    s_password = lv_textarea_create(s_root);
    lv_obj_set_size(s_password, 276, 38);
    lv_obj_align(s_password, LV_ALIGN_TOP_MID, 0, 42);
    lv_obj_set_style_text_font(s_password, &lv_font_montserrat_12, 0);
    lv_textarea_set_password_mode(s_password, true);
    lv_obj_add_flag(s_password, LV_OBJ_FLAG_HIDDEN);
    s_keyboard = lv_keyboard_create(s_root);
    lv_obj_set_size(s_keyboard, 304, 185);
    lv_obj_align(s_keyboard, LV_ALIGN_BOTTOM_MID, 0, -42);
    lv_obj_set_style_text_font(s_keyboard, &lv_font_montserrat_12, LV_PART_ITEMS);
    lv_obj_set_style_pad_all(s_keyboard, 4, 0);
    lv_obj_set_style_pad_row(s_keyboard, 3, 0);
    lv_obj_set_style_pad_column(s_keyboard, 3, 0);
    lv_keyboard_set_mode(s_keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_obj_add_event_cb(s_keyboard, keyboard_event, LV_EVENT_ALL, NULL);
    lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_password);
    lv_obj_move_foreground(s_keyboard);
    s_poll = lv_timer_create(poll_wifi, 400, NULL);
}
