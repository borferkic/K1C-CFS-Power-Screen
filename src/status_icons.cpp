#include "status_icons.h"
#include "powerui.h"

using namespace powerui;

LV_IMG_DECLARE(ui_icon_camera);
LV_IMG_DECLARE(ui_icon_filament);
LV_IMG_DECLARE(network_img);

namespace {
const lv_point_t slash_points[2] = {{3, 3}, {19, 19}};
}

StatusIcons::StatusIcons(lv_obj_t *bar, lv_obj_t *anchor) {
  wifi = create_item(bar, &network_img);
  lv_obj_align_to(wifi.wrapper, anchor, LV_ALIGN_OUT_LEFT_MID, -px(16), 0);

  filament = create_item(bar, &ui_icon_filament);
  lv_obj_align_to(filament.wrapper, wifi.wrapper, LV_ALIGN_OUT_LEFT_MID, -px(12), 0);

  camera = create_item(bar, &ui_icon_camera);
  lv_obj_align_to(camera.wrapper, filament.wrapper, LV_ALIGN_OUT_LEFT_MID, -px(12), 0);

  apply(wifi, false);
  apply(filament, false);
  apply(camera, false);
}

StatusIcons::Item StatusIcons::create_item(lv_obj_t *bar, const lv_img_dsc_t *src) {
  Item item;
  item.wrapper = plain(bar);
  lv_obj_set_size(item.wrapper, px(22), px(22));

  lv_obj_t *ic = icon(item.wrapper, src, 18, lv_color_hex(COLOR_MUTED));
  lv_obj_center(ic);

  // The slash is drawn twice: a wide stroke in the bar color cuts the icon, a thin muted stroke sits on top.
  item.slash_cut = lv_line_create(item.wrapper);
  lv_line_set_points(item.slash_cut, slash_points, 2);
  lv_obj_set_style_line_width(item.slash_cut, 5, 0);
  lv_obj_set_style_line_color(item.slash_cut, lv_color_hex(COLOR_CARD), 0);
  lv_obj_set_style_line_rounded(item.slash_cut, true, 0);

  item.slash = lv_line_create(item.wrapper);
  lv_line_set_points(item.slash, slash_points, 2);
  lv_obj_set_style_line_width(item.slash, 2, 0);
  lv_obj_set_style_line_color(item.slash, lv_color_hex(COLOR_MUTED), 0);
  lv_obj_set_style_line_rounded(item.slash, true, 0);
  return item;
}

void StatusIcons::apply(Item &item, bool active) {
  icon_set_color(lv_obj_get_child(item.wrapper, 0), lv_color_hex(active ? COLOR_ACCENT : COLOR_MUTED));
  if (active) {
    lv_obj_add_flag(item.slash_cut, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(item.slash, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_clear_flag(item.slash_cut, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(item.slash, LV_OBJ_FLAG_HIDDEN);
  }
}

void StatusIcons::set_camera(bool active) { apply(camera, active); }
void StatusIcons::set_filament(bool active) { apply(filament, active); }
void StatusIcons::set_wifi(bool active) { apply(wifi, active); }
