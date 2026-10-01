#include "square_button.h"
#include "powerui.h"

#include <algorithm>

namespace {
constexpr uint32_t ICON_COLOR = 0xFAFAFA;
constexpr uint32_t DISABLED_RED = 0xFF6467;
}

SquareButton::SquareButton(lv_obj_t *parent,
                           const void *button_img,
                           const char *text,
                           lv_event_cb_t cb,
                           void *user_data)
  : button(lv_btn_create(parent))
  , icon_tile(NULL)
  , icon(NULL)
  , text_cont(NULL)
  , label(NULL)
{
  using namespace powerui;
  icon_color = lv_color_hex(ICON_COLOR);
  text_color = lv_color_hex(COLOR_FG);

  lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(button, lv_color_hex(COLOR_CARD), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(button, lv_color_hex(COLOR_SECONDARY), LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_border_width(button, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_color(button, lv_color_white(), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_opa(button, LV_OPA_10, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_radius(button, px(14), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(button, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

  icon_tile = plain(button);
  lv_obj_set_size(icon_tile, px(60), px(60));
  lv_obj_align(icon_tile, LV_ALIGN_TOP_LEFT, px(14), px(14));
  lv_obj_set_style_radius(icon_tile, px(12), 0);
  lv_obj_set_style_bg_color(icon_tile, lv_color_hex(COLOR_SECONDARY), 0);
  lv_obj_set_style_bg_opa(icon_tile, LV_OPA_COVER, 0);

  icon = lv_img_create(icon_tile);
  lv_obj_clear_flag(icon, LV_OBJ_FLAG_CLICKABLE);
  lv_img_set_src(icon, button_img);
  {
    const lv_img_dsc_t *dsc = static_cast<const lv_img_dsc_t *>(button_img);
    const int natural = std::max<int>(dsc->header.w, dsc->header.h);
    lv_img_set_zoom(icon, static_cast<uint16_t>(256 * px(40) / natural));
  }
  lv_obj_center(icon);
  lv_obj_set_style_img_recolor(icon, icon_color, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_img_recolor_opa(icon, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

  // Title and subtitle: a fixed-height block at the bottom so every title sits on the same line.
  text_cont = plain(button);
  lv_obj_set_size(text_cont, LV_PCT(100), px(56));
  lv_obj_align(text_cont, LV_ALIGN_BOTTOM_LEFT, 0, -px(14));
  lv_obj_set_style_pad_left(text_cont, px(14), 0);
  lv_obj_set_style_pad_right(text_cont, px(10), 0);
  lv_obj_set_layout(text_cont, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(text_cont, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(text_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_row(text_cont, px(3), 0);

  label = lv_label_create(text_cont);
  lv_label_set_text(label, text);
  lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(label, LV_PCT(100));
  lv_obj_set_style_text_color(label, text_color, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);

  if (cb != NULL) {
    lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, user_data);
  }
}

SquareButton::~SquareButton() {
}

void SquareButton::set_subtitle(const char *text) {
  if (subtitle == NULL) {
    subtitle = lv_label_create(text_cont);
    lv_label_set_long_mode(subtitle, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(subtitle, LV_PCT(100));
    lv_obj_set_style_text_color(subtitle, lv_color_hex(powerui::COLOR_MUTED), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
  }
  lv_label_set_text(subtitle, text);
}

// Status pill in the top-right corner (e.g. "Offline").
void SquareButton::set_pill(const char *text, lv_color_t dot) {
  if (pill == NULL) {
    pill = powerui::badge(button, text, dot);
    lv_obj_align(pill, LV_ALIGN_TOP_RIGHT, -powerui::px(12), powerui::px(14));
  } else {
    powerui::badge_set(pill, text, dot);
    lv_obj_clear_flag(pill, LV_OBJ_FLAG_HIDDEN);
  }
}

void SquareButton::hide_pill() {
  if (pill != NULL) {
    lv_obj_add_flag(pill, LV_OBJ_FLAG_HIDDEN);
  }
}

lv_obj_t *SquareButton::get_button() {
  return button;
}

void SquareButton::set_icon_color(lv_color_t color) {
  icon_color = color;
  lv_obj_set_style_img_recolor(icon, color, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_img_recolor_opa(icon, LV_OPA_COVER,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
}

void SquareButton::set_background_visible(bool visible) {
  const lv_opa_t opacity = visible ? LV_OPA_COVER : LV_OPA_TRANSP;
  lv_obj_set_style_bg_opa(button, opacity, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(button, opacity, LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(button, opacity, LV_PART_MAIN | LV_STATE_DISABLED);
}

void SquareButton::set_active(bool active, lv_color_t active_color) {
  if (active) {
    lv_obj_set_style_img_recolor(icon, active_color,
                                 LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor_opa(icon, LV_OPA_COVER,
                                     LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(label, active_color,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
  } else {
    set_icon_color(icon_color);
    lv_obj_set_style_text_color(label, text_color,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
  }
}

void SquareButton::disable() {
  lv_obj_add_state(button, LV_STATE_DISABLED);
  lv_obj_add_state(icon, LV_STATE_DISABLED);
  lv_obj_add_state(label, LV_STATE_DISABLED);
  lv_obj_set_style_img_recolor(icon, lv_color_hex(DISABLED_RED),
                               LV_PART_MAIN | LV_STATE_DISABLED);
  lv_obj_set_style_img_recolor_opa(icon, LV_OPA_COVER,
                                   LV_PART_MAIN | LV_STATE_DISABLED);
  lv_obj_set_style_text_color(label, lv_color_hex(powerui::COLOR_MUTED),
                              LV_PART_MAIN | LV_STATE_DISABLED);
  lv_obj_set_style_bg_color(button, lv_color_hex(powerui::COLOR_CARD), LV_PART_MAIN | LV_STATE_DISABLED);
  lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DISABLED);
}

void SquareButton::enable() {
  lv_obj_clear_state(button, LV_STATE_DISABLED);
  lv_obj_clear_state(icon, LV_STATE_DISABLED);
  lv_obj_clear_state(label, LV_STATE_DISABLED);
  set_icon_color(icon_color);
  lv_obj_set_style_text_color(label, text_color,
                              LV_PART_MAIN | LV_STATE_DEFAULT);
}
