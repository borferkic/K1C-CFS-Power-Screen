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
  lv_obj_set_style_border_color(o, lv_color_white(), 0);
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
  lv_obj_set_style_border_color(b, lv_color_white(), 0);
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
  lv_obj_set_style_border_color(t, lv_color_white(), 0);
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
  lv_color_t border = lv_color_white();
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
  lv_obj_set_style_bg_color(btnm, lv_color_hex(0x333333), LV_PART_ITEMS | LV_STATE_PRESSED);
}

} // namespace powerui
