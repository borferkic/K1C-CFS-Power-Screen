#include "wide_button.h"

namespace {
constexpr uint32_t CREALITY_GREEN = 0x4CAF50;
}

WideButton::WideButton(lv_obj_t *parent,
                       const void *button_img,
                       const char *text,
                       lv_event_cb_t cb,
                       void *user_data)
  : button(lv_btn_create(parent))
{
  const lv_color_t green = lv_color_hex(CREALITY_GREEN);

  lv_obj_set_style_bg_color(button, green, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(button, lv_color_darken(green, LV_OPA_20),
                            LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_border_width(button, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_radius(button, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_all(button, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_row(button, 4, LV_PART_MAIN | LV_STATE_DEFAULT);

  lv_obj_set_layout(button, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(button, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(button, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  lv_obj_t *icon = lv_img_create(button);
  lv_img_set_src(icon, button_img);
  lv_obj_set_style_img_recolor(icon, lv_color_white(),
                               LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_img_recolor_opa(icon, LV_OPA_COVER,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);

  lv_obj_t *label = lv_label_create(button);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_color(label, lv_color_white(),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_20,
                             LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER,
                              LV_PART_MAIN | LV_STATE_DEFAULT);

  if (cb != NULL) {
    lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, user_data);
  }
}

WideButton::~WideButton() {
}

lv_obj_t *WideButton::get_button() {
  return button;
}
