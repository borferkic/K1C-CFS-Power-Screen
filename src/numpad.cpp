#include "numpad.h"
#include "powerui.h"
#include "spdlog/spdlog.h"

#include <string>

Numpad::Numpad(lv_obj_t *parent)
  : edit_cont(lv_obj_create(parent))
  , header(lv_obj_create(edit_cont))
  , close_btn(NULL)
  , input(lv_textarea_create(edit_cont))
  , kb(lv_keyboard_create(edit_cont))
  , ready_cb([](double v){})
{
  spdlog::trace("creating numpad on main_cont");
  lv_obj_add_flag(edit_cont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(edit_cont, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_CLICKABLE);

  lv_obj_clear_flag(edit_cont, LV_OBJ_FLAG_SCROLLABLE);
  
  lv_obj_move_background(edit_cont);
  lv_obj_set_size(edit_cont, LV_PCT(48), LV_PCT(100));

  lv_obj_set_flex_align(edit_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_align(edit_cont, LV_ALIGN_RIGHT_MID, 0, 0);

  // PowerUI card: dark surface, thin border, rounded corners.
  using namespace powerui;
  lv_obj_set_style_bg_color(edit_cont, lv_color_hex(COLOR_CARD), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(edit_cont, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_color(edit_cont, lv_color_hex(COLOR_WHITE), LV_PART_MAIN);
  lv_obj_set_style_border_opa(edit_cont, LV_OPA_20, LV_PART_MAIN);
  lv_obj_set_style_border_width(edit_cont, 1, LV_PART_MAIN);
  lv_obj_set_style_radius(edit_cont, px(14), LV_PART_MAIN);
  lv_obj_set_style_shadow_width(edit_cont, 0, LV_PART_MAIN);

  lv_obj_set_flex_flow(edit_cont, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(edit_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_all(edit_cont, px(14), 0);
  lv_obj_set_style_pad_row(edit_cont, px(10), 0);

  // Header: title on the left, close button (X) on the right.
  lv_obj_remove_style_all(header);
  lv_obj_set_size(header, LV_PCT(100), px(40));
  lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t *title = label(header, "Set temperature", &lv_font_montserrat_18, lv_color_hex(COLOR_FG));
  lv_obj_align(title, LV_ALIGN_LEFT_MID, px(4), 0);
  close_btn = lv_btn_create(header);
  lv_obj_set_size(close_btn, px(40), px(40));
  lv_obj_align(close_btn, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_set_style_radius(close_btn, px(10), LV_PART_MAIN);
  lv_obj_set_style_shadow_width(close_btn, 0, LV_PART_MAIN);
  lv_obj_set_style_border_width(close_btn, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_color(close_btn, lv_color_hex(COLOR_SECONDARY), LV_PART_MAIN);
  lv_obj_set_style_bg_color(close_btn, lv_color_hex(COLOR_PRESSED), LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_t *close_icon = label(close_btn, LV_SYMBOL_CLOSE, &lv_font_montserrat_18, lv_color_hex(COLOR_FG));
  lv_obj_center(close_icon);
  lv_obj_add_event_cb(close_btn, &Numpad::_handle_close, LV_EVENT_CLICKED, this);
  lv_obj_move_to_index(header, 0);

  lv_obj_set_size(input, LV_PCT(100), LV_SIZE_CONTENT);
  lv_textarea_set_one_line(input, true);
  lv_obj_set_style_bg_color(input, lv_color_hex(COLOR_SECONDARY), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(input, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(input, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(input, px(10), LV_PART_MAIN);
  lv_obj_set_style_text_color(input, lv_color_hex(COLOR_FG), LV_PART_MAIN);
  lv_obj_set_style_text_font(input, &lv_font_montserrat_24, LV_PART_MAIN);

  lv_obj_set_width(kb, LV_PCT(100));
  lv_obj_set_flex_grow(kb, 1);
  static const char * kb_map[] = {"1", "2", "3", "\n", "4", "5", "6", "\n", "7", "8", "9", "\n", LV_SYMBOL_BACKSPACE, "0", LV_SYMBOL_OK, NULL };
  static const lv_btnmatrix_ctrl_t kb_ctrl[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
  lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_USER_1, kb_map, kb_ctrl);

  lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_USER_1);
  lv_keyboard_set_textarea(kb, input);
  // Keys: dark rounded buttons, the OK key in green.
  lv_obj_set_style_bg_opa(kb, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(kb, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(kb, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_gap(kb, px(8), LV_PART_MAIN);
  lv_obj_set_style_radius(kb, px(10), LV_PART_ITEMS);
  lv_obj_set_style_border_width(kb, 0, LV_PART_ITEMS);
  lv_obj_set_style_shadow_width(kb, 0, LV_PART_ITEMS);
  lv_obj_set_style_bg_color(kb, lv_color_hex(COLOR_SECONDARY), LV_PART_ITEMS);
  lv_obj_set_style_bg_opa(kb, LV_OPA_COVER, LV_PART_ITEMS);
  lv_obj_set_style_text_color(kb, lv_color_hex(COLOR_FG), LV_PART_ITEMS);
  lv_obj_set_style_text_font(kb, &lv_font_montserrat_20, LV_PART_ITEMS);
  lv_obj_set_style_bg_color(kb, lv_color_hex(COLOR_PRESSED), LV_PART_ITEMS | LV_STATE_PRESSED);
  lv_obj_set_style_bg_color(kb, lv_color_hex(COLOR_PRIMARY), LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_text_color(kb, lv_color_hex(COLOR_WHITE), LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_btnmatrix_set_btn_ctrl(kb, 11, LV_BTNMATRIX_CTRL_CHECKED);  // OK

  lv_obj_add_event_cb(input, &Numpad::_handle_input, LV_EVENT_ALL, this);
  // lv_obj_add_event_cb(edit_cont, &Numpad::_handle_defocused, LV_EVENT_DEFOCUSED, this);
}

Numpad::~Numpad() {
  if (edit_cont != NULL) {
    lv_obj_del(edit_cont);
    edit_cont = NULL;
  }
}

// Hides the panel without sending a temperature (close button).
void Numpad::close() {
  lv_obj_add_flag(edit_cont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_background(edit_cont);
  lv_textarea_set_text(input, "");
}

void Numpad::set_callback(std::function<void(double)> cb) {
  ready_cb = cb;
}

void Numpad::handle_input(lv_event_t *e) {
  const lv_event_code_t code = lv_event_get_code(e);

  // if(code == LV_EVENT_FOCUSED) {
  //   spdlog::debug("input focused");
  //   lv_keyboard_set_textarea(kb, input);
  //   // lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
  // }
  

  // if(code == LV_EVENT_DEFOCUSED) {
  //   // lv_keyboard_set_textarea(kb, NULL);
  //   // lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
  //   spdlog::debug("input defocused");
  //   lv_obj_add_flag(edit_cont, LV_OBJ_FLAG_HIDDEN);
  //   lv_obj_move_background(edit_cont);
  //   lv_textarea_set_text(input, "");
  //   lv_keyboard_set_textarea(kb, NULL);
  // }

  if (code == LV_EVENT_VALUE_CHANGED) {
    // input validation, e.g. range
  }

  if (code == LV_EVENT_CANCEL) {
    lv_obj_add_flag(edit_cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_background(edit_cont);
  }
  
  if (code == LV_EVENT_READY) {
    // input validation, e.g. range
    std::string value = std::string(lv_textarea_get_text(input));
    if (value.length() > 0) {
      ready_cb(std::stod(value));
    }

    lv_obj_add_flag(edit_cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_background(edit_cont);
    lv_textarea_set_text(input, "");
  }
}

// void Numpad::handle_defocused(lv_event_t *e) {
//   const lv_event_code_t code = lv_event_get_code(e);

//   if (code == LV_EVENT_DEFOCUSED) {
//     spdlog::debug("numpad group defocused");
//     lv_obj_add_flag(edit_cont, LV_OBJ_FLAG_HIDDEN);
//     lv_obj_move_background(edit_cont);
//     lv_textarea_set_text(input, "");
//   }
// }

void Numpad::foreground_reset() {
  spdlog::trace("resetting foreground");
  lv_textarea_set_text(input, "");
  lv_obj_clear_flag(edit_cont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(edit_cont);
}
