#include "powerui.h"

#include <algorithm>
#include <cmath>

LV_IMG_DECLARE(print);
LV_IMG_DECLARE(chart_img);

namespace powerui {

double scale() {
  const double s = (double)lv_disp_get_physical_hor_res(NULL) / 800.0;
  return s > 0.0 ? s : 1.0;
}

lv_coord_t px(int design_px) {
  return static_cast<lv_coord_t>(std::lround(design_px * scale()));
}

lv_coord_t overlay_width_px() {
  return lv_disp_get_physical_hor_res(NULL) - px(64);
}

lv_coord_t overlay_height_px() {
  return lv_disp_get_physical_ver_res(NULL) - px(40);
}

double overlay_width_scale() {
  return static_cast<double>(overlay_width_px()) / 800.0;
}

double overlay_height_scale() {
  return static_cast<double>(overlay_height_px()) / 480.0;
}

namespace {
std::function<void(const std::string &, std::function<void()>)> overlay_handler;
}

void set_overlay_handlers(std::function<void(const std::string &, std::function<void()>)> open_handler) {
  overlay_handler = open_handler;
}

void overlay_open(const std::string &title, std::function<void()> on_back) {
  if (overlay_handler) {
    overlay_handler(title, on_back);
  }
}

lv_obj_t *plain(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
  return o;
}

lv_obj_t *card(lv_obj_t *parent, int x, int y, int w, int h) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_pos(o, px(x), px(y));
  lv_obj_set_size(o, px(w), px(h));
  lv_obj_set_style_bg_color(o, lv_color_hex(COLOR_CARD), 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(o, px(14), 0);
  lv_obj_set_style_border_width(o, 1, 0);
  lv_obj_set_style_border_color(o, lv_color_hex(COLOR_WHITE), 0);
  lv_obj_set_style_border_opa(o, LV_OPA_10, 0);
  return o;
}

lv_obj_t *label(lv_obj_t *parent, const char *text, const lv_font_t *font, lv_color_t color) {
  lv_obj_t *l = lv_label_create(parent);
  lv_label_set_text(l, text);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, color, 0);
  return l;
}

lv_obj_t *icon(lv_obj_t *parent, const lv_img_dsc_t *src, int size_px, lv_color_t color) {
  lv_obj_t *wrapper = plain(parent);
  lv_obj_set_size(wrapper, px(size_px), px(size_px));

  lv_obj_t *img = lv_img_create(wrapper);
  lv_obj_clear_flag(img, LV_OBJ_FLAG_CLICKABLE);
  lv_img_set_src(img, src);
  const int natural = std::max<int>(src->header.w, src->header.h);
  lv_img_set_zoom(img, static_cast<uint16_t>(256 * px(size_px) / natural));
  lv_obj_set_style_img_recolor(img, color, 0);
  lv_obj_set_style_img_recolor_opa(img, LV_OPA_COVER, 0);
  lv_obj_center(img);
  return wrapper;
}

void icon_set_color(lv_obj_t *wrapper, lv_color_t color) {
  lv_obj_t *img = lv_obj_get_child(wrapper, 0);
  if (img != NULL) {
    lv_obj_set_style_img_recolor(img, color, 0);
  }
}

lv_obj_t *badge(lv_obj_t *parent, const char *text, lv_color_t dot_color) {
  lv_obj_t *b = plain(parent);
  lv_obj_set_size(b, LV_SIZE_CONTENT, px(24));
  lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(b, 1, 0);
  lv_obj_set_style_border_color(b, lv_color_hex(COLOR_WHITE), 0);
  lv_obj_set_style_border_opa(b, LV_OPA_10, 0);
  lv_obj_set_style_pad_left(b, px(10), 0);
  lv_obj_set_style_pad_right(b, px(10), 0);
  lv_obj_set_style_pad_column(b, px(6), 0);
  lv_obj_set_flex_flow(b, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_t *dot = plain(b);
  lv_obj_set_size(dot, px(7), px(7));
  lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(dot, dot_color, 0);

  label(b, text, &lv_font_montserrat_12, lv_color_hex(COLOR_FG));
  return b;
}

void badge_set(lv_obj_t *b, const char *text, lv_color_t dot_color) {
  lv_obj_set_style_bg_color(lv_obj_get_child(b, 0), dot_color, 0);
  lv_label_set_text(lv_obj_get_child(b, 1), text);
}

lv_obj_t *view_toggle(lv_obj_t *parent, lv_event_cb_t cb, void *user_data) {
  lv_obj_t *t = plain(parent);
  lv_obj_set_size(t, px(76), px(36));
  lv_obj_set_style_radius(t, px(10), 0);
  lv_obj_set_style_bg_color(t, lv_color_hex(COLOR_SECONDARY), 0);
  lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(t, 1, 0);
  lv_obj_set_style_border_color(t, lv_color_hex(COLOR_WHITE), 0);
  lv_obj_set_style_border_opa(t, LV_OPA_10, 0);

  const lv_img_dsc_t *icons[2] = {&print, &chart_img};
  for (int i = 0; i < 2; ++i) {
    lv_obj_t *btn = plain(t);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(btn, px(34), px(30));
    lv_obj_set_pos(btn, px(2 + i * 36), px(2));
    lv_obj_set_style_radius(btn, px(8), 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(COLOR_ACCENT), 0);
    lv_obj_t *ic = icon(btn, icons[i], 20, lv_color_hex(COLOR_MUTED));
    lv_obj_center(ic);
    if (cb != NULL) {
      lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
    }
  }
  view_toggle_set(t, true);
  return t;
}

void view_toggle_set(lv_obj_t *t, bool print_view) {
  for (int i = 0; i < 2; ++i) {
    lv_obj_t *btn = lv_obj_get_child(t, i);
    const bool active = (i == 0) == print_view;
    lv_obj_set_style_bg_opa(btn, active ? LV_OPA_20 : LV_OPA_TRANSP, 0);
    icon_set_color(lv_obj_get_child(btn, 0),
                   lv_color_hex(active ? COLOR_ACCENT : COLOR_MUTED));
  }
}

void style_button(lv_obj_t *container, lv_obj_t *inner, ButtonKind kind) {
  lv_color_t main = lv_color_hex(COLOR_FG);
  lv_color_t border = lv_color_hex(COLOR_WHITE);
  lv_opa_t border_opa = LV_OPA_10;
  lv_color_t bg = lv_color_hex(COLOR_CARD);
  lv_opa_t bg_opa = LV_OPA_COVER;
  lv_color_t bg_pressed = lv_color_hex(COLOR_SECONDARY);
  lv_opa_t bg_pressed_opa = LV_OPA_COVER;

  if (kind == ButtonKind::Soft) {
    main = lv_color_hex(COLOR_ACCENT);
    border = main;
    border_opa = LV_OPA_50;
    bg = main;
    bg_opa = LV_OPA_10 + 11;
    bg_pressed = main;
    bg_pressed_opa = LV_OPA_30;
  } else if (kind == ButtonKind::Destructive) {
    main = lv_color_hex(COLOR_DESTRUCTIVE);
    border = main;
    border_opa = LV_OPA_40;
  }

  lv_obj_set_style_radius(container, px(10), LV_PART_MAIN);
  lv_obj_set_style_bg_color(container, bg, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(container, bg_opa, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(container, bg_pressed, LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(container, bg_pressed_opa, LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_border_width(container, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(container, border, LV_PART_MAIN);
  lv_obj_set_style_border_opa(container, border_opa, LV_PART_MAIN);
  lv_obj_set_style_text_color(container, main, LV_PART_MAIN);
  lv_obj_set_style_opa(container, LV_OPA_50, LV_PART_MAIN | LV_STATE_DISABLED);
  if (inner != NULL) {
    lv_obj_set_style_bg_opa(inner, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(inner, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(inner, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DISABLED);
    lv_obj_set_style_img_recolor(inner, main, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor_opa(inner, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
  }
}

void style_segmented(lv_obj_t *btnm) {
  lv_obj_set_style_bg_opa(btnm, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(btnm, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(btnm, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_row(btnm, px(6), LV_PART_MAIN);
  lv_obj_set_style_pad_column(btnm, px(6), LV_PART_MAIN);
  lv_obj_set_style_outline_width(btnm, 0, LV_PART_ITEMS);
  lv_obj_set_style_shadow_width(btnm, 0, LV_PART_ITEMS);
  lv_obj_set_style_radius(btnm, px(8), LV_PART_ITEMS);
  lv_obj_set_style_border_width(btnm, 0, LV_PART_ITEMS);
  lv_obj_set_style_bg_color(btnm, lv_color_hex(COLOR_SECONDARY), LV_PART_ITEMS);
  lv_obj_set_style_bg_opa(btnm, LV_OPA_COVER, LV_PART_ITEMS);
  lv_obj_set_style_text_color(btnm, lv_color_hex(COLOR_FG), LV_PART_ITEMS);
  lv_obj_set_style_text_font(btnm, &lv_font_montserrat_16, LV_PART_ITEMS);

  lv_obj_set_style_bg_color(btnm, lv_color_hex(COLOR_ACCENT), LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_bg_opa(btnm, LV_OPA_20, LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_border_width(btnm, 1, LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_border_color(btnm, lv_color_hex(COLOR_ACCENT), LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_border_opa(btnm, LV_OPA_50, LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_text_color(btnm, lv_color_hex(COLOR_ACCENT), LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_bg_color(btnm, lv_color_hex(COLOR_PRESSED), LV_PART_ITEMS | LV_STATE_PRESSED);
}

lv_obj_t *empty_state(lv_obj_t *parent, const lv_img_dsc_t *icon_src, const char *title, const char *description) {
  lv_obj_t *box = plain(parent);
  lv_obj_set_width(box, LV_PCT(100));
  lv_obj_set_height(box, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_top(box, px(48), 0);
  lv_obj_set_style_pad_row(box, px(8), 0);

  lv_obj_t *tile = plain(box);
  lv_obj_set_size(tile, px(56), px(56));
  lv_obj_set_style_radius(tile, px(12), 0);
  lv_obj_set_style_bg_color(tile, lv_color_hex(COLOR_SECONDARY), 0);
  lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
  lv_obj_t *glyph = icon(tile, icon_src, 32, lv_color_hex(COLOR_MUTED));
  lv_obj_center(glyph);

  lv_obj_t *title_label = label(box, title, &lv_font_montserrat_16, lv_color_hex(COLOR_FG));
  lv_obj_set_style_text_align(title_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_t *description_label = label(box, description, &lv_font_montserrat_12, lv_color_hex(COLOR_MUTED));
  lv_label_set_long_mode(description_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(description_label, LV_PCT(80));
  lv_obj_set_style_text_align(description_label, LV_TEXT_ALIGN_CENTER, 0);
  return box;
}

void style_select(lv_obj_t *dropdown) {
  lv_obj_set_style_bg_color(dropdown, lv_color_hex(COLOR_CARD), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(dropdown, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(dropdown, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(dropdown, lv_color_hex(COLOR_WHITE), LV_PART_MAIN);
  lv_obj_set_style_border_opa(dropdown, LV_OPA_10, LV_PART_MAIN);
  lv_obj_set_style_radius(dropdown, px(10), LV_PART_MAIN);
  lv_obj_set_style_shadow_width(dropdown, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_hor(dropdown, px(12), LV_PART_MAIN);
  lv_obj_set_style_text_color(dropdown, lv_color_hex(COLOR_FG), LV_PART_MAIN);
  lv_obj_set_style_text_font(dropdown, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_set_style_bg_color(dropdown, lv_color_hex(COLOR_SECONDARY), LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_border_color(dropdown, lv_color_hex(COLOR_ACCENT), LV_PART_MAIN | LV_STATE_FOCUSED);
  lv_obj_set_style_border_opa(dropdown, LV_OPA_50, LV_PART_MAIN | LV_STATE_FOCUSED);
  lv_obj_set_style_outline_width(dropdown, 0, LV_PART_MAIN);
  lv_obj_set_style_text_color(dropdown, lv_color_hex(COLOR_MUTED), LV_PART_INDICATOR);

  // Open list.
  lv_obj_t *list = lv_dropdown_get_list(dropdown);
  if (list != NULL) {
    lv_obj_set_style_bg_color(list, lv_color_hex(COLOR_CARD), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(list, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(list, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(list, lv_color_hex(COLOR_WHITE), LV_PART_MAIN);
    lv_obj_set_style_border_opa(list, LV_OPA_20, LV_PART_MAIN);
    lv_obj_set_style_radius(list, px(10), LV_PART_MAIN);
    lv_obj_set_style_shadow_width(list, 0, LV_PART_MAIN);
    lv_obj_set_style_text_color(list, lv_color_hex(COLOR_FG), LV_PART_MAIN);
    lv_obj_set_style_text_font(list, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(list, px(6), LV_PART_MAIN);
    lv_obj_set_style_text_line_space(list, px(14), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, LV_PART_SELECTED);
    lv_obj_set_style_bg_color(list, lv_color_hex(COLOR_ACCENT), LV_PART_SELECTED | LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(list, LV_OPA_20, LV_PART_SELECTED | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(list, lv_color_hex(COLOR_ACCENT), LV_PART_SELECTED | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(list, lv_color_hex(COLOR_SECONDARY), LV_PART_SELECTED | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(list, LV_OPA_COVER, LV_PART_SELECTED | LV_STATE_PRESSED);
  }
}

void style_slider(lv_obj_t *slider) {
  lv_obj_set_style_bg_color(slider, lv_color_hex(COLOR_SECONDARY), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(slider, lv_color_hex(COLOR_ACCENT), LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(slider, lv_color_hex(COLOR_FG), LV_PART_KNOB);
  lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_KNOB);
  lv_obj_set_style_pad_all(slider, px(6), LV_PART_KNOB);
  lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_MAIN);
  lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
  lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_KNOB);
  lv_obj_set_style_shadow_width(slider, 0, LV_PART_KNOB);
}

void style_switch(lv_obj_t *sw) {
  lv_obj_set_size(sw, px(46), px(26));
  lv_obj_set_style_bg_color(sw, lv_color_hex(COLOR_SECONDARY), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(sw, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(sw, lv_color_hex(COLOR_WHITE), LV_PART_MAIN);
  lv_obj_set_style_border_opa(sw, LV_OPA_10, LV_PART_MAIN);
  lv_obj_set_style_bg_color(sw, lv_color_hex(COLOR_SECONDARY), LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(sw, lv_color_hex(COLOR_ACCENT), LV_PART_INDICATOR | LV_STATE_CHECKED);
  lv_obj_set_style_bg_color(sw, lv_color_hex(COLOR_FG), LV_PART_KNOB);
  lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, LV_PART_KNOB);
  lv_obj_set_style_bg_color(sw, lv_color_hex(COLOR_ACCENT_DARK), LV_PART_KNOB | LV_STATE_CHECKED);
  lv_obj_set_style_pad_all(sw, px(-3), LV_PART_KNOB);
  lv_obj_set_style_shadow_width(sw, 0, LV_PART_KNOB);
  lv_obj_set_style_outline_width(sw, 0, LV_PART_MAIN);
}

lv_obj_t *action_button(lv_obj_t *parent, const lv_img_dsc_t *icon_src, const char *text, ActionKind kind,
                        int x, int y, int w, int h, lv_event_cb_t cb, void *user_data) {
  lv_obj_t *btn = lv_btn_create(parent);
  lv_obj_set_pos(btn, px(x), px(y));
  lv_obj_set_size(btn, px(w), px(h));
  lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_all(btn, 0, 0);
  lv_obj_set_style_pad_column(btn, px(8), 0);
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_set_style_radius(btn, px(10), 0);
  lv_obj_set_style_border_width(btn, 1, 0);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);

  lv_color_t main = lv_color_hex(COLOR_FG);
  if (kind == ActionKind::Primary) {
    main = lv_color_hex(COLOR_ACCENT_DARK);
    lv_obj_set_style_bg_color(btn, lv_color_hex(COLOR_ACCENT), 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(COLOR_ACCENT_PRESSED), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(btn, 0, 0);
  } else {
    if (kind == ActionKind::Destructive) {
      main = lv_color_hex(COLOR_DESTRUCTIVE);
      lv_obj_set_style_border_color(btn, main, 0);
      lv_obj_set_style_border_opa(btn, LV_OPA_40, 0);
    } else {
      lv_obj_set_style_border_color(btn, lv_color_hex(COLOR_WHITE), 0);
      lv_obj_set_style_border_opa(btn, LV_OPA_10, 0);
    }
    lv_obj_set_style_bg_color(btn, lv_color_hex(COLOR_CARD), 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(COLOR_SECONDARY), LV_STATE_PRESSED);
  }
  lv_obj_set_style_opa(btn, LV_OPA_50, LV_STATE_DISABLED);

  if (icon_src != NULL) {
    lv_obj_t *ic = icon(btn, icon_src, 20, main);
    lv_obj_clear_flag(ic, LV_OBJ_FLAG_CLICKABLE);
  }
  lv_obj_t *l = label(btn, text, &lv_font_montserrat_16, main);
  lv_obj_clear_flag(l, LV_OBJ_FLAG_CLICKABLE);
  if (cb != NULL) {
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
  }
  return btn;
}

void style_overlay_root(lv_obj_t *cont) {
  lv_obj_set_style_pad_all(cont, 0, 0);
  lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(cont, lv_color_hex(COLOR_BG), 0);
  lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(cont, 0, 0);
  lv_obj_set_style_radius(cont, 0, 0);
}

} // namespace powerui
