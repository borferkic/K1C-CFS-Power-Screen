#include "square_button.h"

namespace {
constexpr uint32_t SETTINGS_BUTTON_GREY = 0x555555;
constexpr uint32_t CREALITY_GREEN = 0x4CAF50;
constexpr uint32_t DISABLED_RED = 0xF44336;
}

SquareButton::SquareButton(lv_obj_t *parent,
                           const void *button_img,
                           const char *text,
                           lv_event_cb_t cb,
                           void *user_data)
  : button(lv_btn_create(parent))
  , icon(lv_img_create(button))
  , label(lv_label_create(button))
{
  const lv_color_t button_grey = lv_color_hex(SETTINGS_BUTTON_GREY);
  const lv_color_t green = lv_color_hex(CREALITY_GREEN);
  icon_color = green;
  text_color = lv_color_white();

  lv_obj_set_size(button, 150, 150);
  lv_obj_set_style_bg_color(button, button_grey, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(button, lv_color_darken(button_grey, LV_OPA_20),
                            LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_border_width(button, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_radius(button, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_all(button, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_row(button, 6, LV_PART_MAIN | LV_STATE_DEFAULT);

  lv_obj_set_layout(button, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(button, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(button, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  lv_img_set_src(icon, button_img);
  lv_obj_set_style_img_recolor(icon, icon_color, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_img_recolor_opa(icon, LV_OPA_COVER,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);

  lv_label_set_text(label, text);
  lv_obj_set_width(label, LV_PCT(100));
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER,
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(label, text_color,
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_16,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  if (cb != NULL) {
    lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, user_data);
  }
}

SquareButton::~SquareButton() {
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
  lv_obj_set_style_text_color(label, text_color,
                              LV_PART_MAIN | LV_STATE_DISABLED);
}

void SquareButton::enable() {
  lv_obj_clear_state(button, LV_STATE_DISABLED);
  lv_obj_clear_state(icon, LV_STATE_DISABLED);
  lv_obj_clear_state(label, LV_STATE_DISABLED);
  set_icon_color(icon_color);
  lv_obj_set_style_text_color(label, text_color,
                              LV_PART_MAIN | LV_STATE_DEFAULT);
}
