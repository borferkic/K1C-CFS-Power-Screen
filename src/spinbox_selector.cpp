#include "spinbox_selector.h"
#include "powerui.h"

SpinBoxSelector::SpinBoxSelector(lv_obj_t *parent,
				 const std::string &name,
				 int min,
				 int max,
				 int value,
				 std::function<void(int)> update_cb)
  : cont(lv_obj_create(parent))
  , sb_cont(lv_obj_create(cont))
  , sb(lv_spinbox_create(sb_cont))
  , cb(update_cb)
{
  lv_obj_set_size(cont, LV_PCT(48), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN_REVERSE);
  lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(cont, lv_color_hex(powerui::COLOR_SECONDARY), 0);
  lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(cont, 6, 0);
  lv_obj_set_style_border_width(cont, 1, 0);
  lv_obj_set_style_border_color(cont, lv_color_hex(powerui::COLOR_WHITE), 0);
  lv_obj_set_style_border_opa(cont, LV_OPA_10, 0);
  lv_obj_set_style_pad_all(cont, 4, 0);
  lv_obj_set_style_pad_row(cont, 2, 0);
  
  lv_obj_set_size(sb_cont, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_style_pad_all(sb_cont, 0, 0);

  lv_obj_t *l = lv_label_create(cont);
  lv_obj_set_width(l, LV_PCT(100));
  lv_label_set_text(l, name.c_str());
  lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(powerui::COLOR_MUTED), 0);

  lv_obj_center(sb);

  lv_obj_set_size(sb, LV_PCT(40), LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(sb, LV_OPA_0, 0);
  lv_obj_set_style_border_width(sb, 0, 0);
  lv_obj_set_style_text_color(sb, lv_color_hex(powerui::COLOR_FG), 0);
  // lv_obj_set_style_pad_all(sb, 4, 0);
  lv_spinbox_set_range(sb, min, max);
  lv_spinbox_set_value(sb, value);
  lv_spinbox_set_step(sb, 1);
  lv_spinbox_set_digit_format(sb, 2, 0);
  lv_obj_set_style_border_width(sb, 0, LV_PART_CURSOR);
  lv_obj_set_style_border_opa(sb, LV_OPA_0, LV_PART_CURSOR);
  lv_obj_set_style_bg_opa(sb, LV_OPA_0, LV_PART_CURSOR);
  lv_textarea_set_cursor_click_pos(sb, false);

  int32_t h = lv_obj_get_height(sb);
  lv_obj_t * btn = lv_btn_create(sb_cont);
  lv_obj_set_size(btn, h, h);
  lv_obj_align(btn, LV_ALIGN_RIGHT_MID, 0, 0);
  
  lv_obj_set_style_bg_img_src(btn, LV_SYMBOL_PLUS, 0);
  lv_obj_add_event_cb(btn, [](lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    SpinBoxSelector *panel = (SpinBoxSelector*)e->user_data;
    if(code == LV_EVENT_LONG_PRESSED_REPEAT) {
      lv_spinbox_increment(panel->sb);
    }

    if (code == LV_EVENT_RELEASED) {
      lv_spinbox_increment(panel->sb);
      if (panel->cb) {
	panel->cb(lv_spinbox_get_value(panel->sb));
      }
    }
      
  }, LV_EVENT_ALL, this);

  btn = lv_btn_create(sb_cont);
  lv_obj_set_size(btn, h, h);
  lv_obj_align(btn, LV_ALIGN_LEFT_MID, 0, 0);

  lv_obj_set_style_bg_img_src(btn, LV_SYMBOL_MINUS, 0);
  lv_obj_add_event_cb(btn, [](lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    SpinBoxSelector *panel = (SpinBoxSelector*)e->user_data;    
    if(code == LV_EVENT_LONG_PRESSED_REPEAT) {
      lv_spinbox_decrement(panel->sb);
    }

    if (code == LV_EVENT_RELEASED) {
      lv_spinbox_decrement(panel->sb);
      if (panel->cb) {
	panel->cb(lv_spinbox_get_value(panel->sb));
      }
    }
  }, LV_EVENT_ALL, this);
}

SpinBoxSelector::~SpinBoxSelector() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

void SpinBoxSelector::update_value(int v) {
  lv_spinbox_set_value(sb, v);
}
