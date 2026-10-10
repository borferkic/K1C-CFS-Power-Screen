#include "macro_item.h"
#include "powerui.h"

MacroItem::MacroItem(lv_obj_t *parent,
		     const std::string &name,
		     const std::map<std::string, std::string> &params,
		     lv_coord_t tile_width,
		     bool hide,
		     Callback on_tap,
		     Callback on_long_press)
  : cont(powerui::card(parent, 0, 0, 10, 10))
  , macro_name(name)
  , macro_params(params)
  , hidden(hide)
  , tap_cb(std::move(on_tap))
  , long_press_cb(std::move(on_long_press))
{
  using namespace powerui;

  lv_obj_set_size(cont, tile_width, px(84));
  lv_obj_add_flag(cont, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_color(cont, lv_color_hex(COLOR_SECONDARY), LV_STATE_PRESSED);

  lv_obj_t *title = label(cont, macro_name.c_str(), &lv_font_montserrat_14, lv_color_hex(COLOR_FG));
  lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
  lv_obj_set_size(title, tile_width - px(24), px(36));
  lv_obj_set_pos(title, px(12), px(10));

  const std::string info = macro_params.empty()
    ? "No parameters"
    : std::to_string(macro_params.size()) + (macro_params.size() == 1 ? " parameter" : " parameters");
  lv_obj_t *info_label = label(cont, info.c_str(), &lv_font_montserrat_12, lv_color_hex(COLOR_MUTED));
  lv_obj_align(info_label, LV_ALIGN_BOTTOM_LEFT, px(12), -px(10));

  lv_obj_t *mark = label(cont, macro_params.empty() ? LV_SYMBOL_PLAY : LV_SYMBOL_SETTINGS, &lv_font_montserrat_16,
			 lv_color_hex(COLOR_ACCENT));
  lv_obj_align(mark, LV_ALIGN_BOTTOM_RIGHT, -px(12), -px(8));

  lv_obj_add_event_cb(cont, &MacroItem::_handle_event, LV_EVENT_ALL, this);
  set_hidden(hidden);
}

MacroItem::~MacroItem() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

void MacroItem::handle_event(lv_event_t *e) {
  const lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_SHORT_CLICKED && tap_cb) {
    tap_cb(*this);
  } else if (code == LV_EVENT_LONG_PRESSED && long_press_cb) {
    long_press_cb(*this);
  }
}

void MacroItem::set_hidden(bool hide) {
  hidden = hide;
  lv_obj_set_style_opa(cont, hidden ? LV_OPA_50 : LV_OPA_COVER, 0);
}

void MacroItem::update_visibility(bool matches_search, bool show_hidden) {
  if (matches_search && (!hidden || show_hidden)) {
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(cont, LV_OBJ_FLAG_HIDDEN);
  }
}
