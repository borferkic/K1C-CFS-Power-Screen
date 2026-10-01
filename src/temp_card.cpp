#include "temp_card.h"
#include "powerui.h"
#include "spdlog/spdlog.h"

using namespace powerui;

TempCard::TempCard(KWebSocketClient &c,
                   lv_obj_t *parent,
                   int x, int y, int w, int h,
                   const lv_img_dsc_t *icon_src,
                   const char *name,
                   lv_color_t color,
                   bool editable,
                   Numpad &np,
                   std::string name_id,
                   lv_obj_t *chart_chart,
                   lv_chart_series_t *chart_series)
  : ws(c)
  , cont(card(parent, x, y, w, h))
  , value_label(NULL)
  , target_label(NULL)
  , value(-1)
  , target(-1)
  , numpad(np)
  , id(name_id)
  , chart(chart_chart)
  , series(chart_series)
  , last_updated_ts(std::time(nullptr))
{
  // Layout inside the card (design px): 14 air | 4x32 color bar | 12 | 40x40 icon tile | 12 | name ... value / target | 16
  const int content_h = h - 2;

  lv_obj_t *bar = plain(cont);
  lv_obj_set_size(bar, px(4), px(32));
  lv_obj_set_pos(bar, px(13), px((content_h - 32) / 2));
  lv_obj_set_style_radius(bar, px(2), 0);
  lv_obj_set_style_bg_color(bar, color, 0);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);

  lv_obj_t *tile = plain(cont);
  lv_obj_set_size(tile, px(40), px(40));
  lv_obj_set_pos(tile, px(29), px((content_h - 40) / 2));
  lv_obj_set_style_radius(tile, px(10), 0);
  lv_obj_set_style_bg_color(tile, lv_color_hex(COLOR_SECONDARY), 0);
  lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
  lv_obj_t *ic = icon(tile, icon_src, 26, color);
  lv_obj_center(ic);

  lv_obj_t *name_label = label(cont, name, &lv_font_montserrat_14, lv_color_hex(COLOR_MUTED));
  lv_obj_set_width(name_label, px(100));
  lv_label_set_long_mode(name_label, LV_LABEL_LONG_DOT);
  lv_obj_align(name_label, LV_ALIGN_LEFT_MID, px(81), 0);

  target_label = label(cont, "/ --", &lv_font_montserrat_14, lv_color_hex(COLOR_MUTED));
  lv_obj_set_width(target_label, px(46));
  lv_label_set_long_mode(target_label, LV_LABEL_LONG_CLIP);
  lv_obj_align(target_label, LV_ALIGN_RIGHT_MID, -px(15), px(2));

  value_label = label(cont, "--", &lv_font_montserrat_24, lv_color_hex(COLOR_FG));
  lv_obj_set_width(value_label, px(76));
  lv_obj_set_style_text_align(value_label, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_align_to(value_label, target_label, LV_ALIGN_OUT_LEFT_MID, -px(4), -px(2));

  if (editable) {
    lv_obj_add_flag(cont, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(cont, lv_color_hex(COLOR_SECONDARY), LV_STATE_PRESSED);
    lv_obj_add_event_cb(cont, &TempCard::_handle_edit, LV_EVENT_CLICKED, this);
  }
}

TempCard::~TempCard() {
  if (cont != NULL) {
    spdlog::debug("deleting temp card {}", id);
    lv_obj_del(cont);
    cont = NULL;
  }

  if (series != NULL && chart != NULL) {
    lv_chart_remove_series(chart, series);
    series = NULL;
  }
}

void TempCard::update_target(int new_target) {
  if (new_target < 0) {
    return;
  }
  target = new_target;
  if (new_target > 0) {
    lv_label_set_text(target_label, fmt::format("/ {}°", new_target).c_str());
  } else {
    lv_label_set_text(target_label, "/ --");
  }
}

void TempCard::update_value(int new_value) {
  if (value != new_value) {
    value = new_value;
    lv_label_set_text(value_label, fmt::format("{}°", new_value).c_str());
  }
}

void TempCard::update_series(int v) {
  if (series != NULL && chart != NULL) {
    auto delta = std::time(nullptr) - last_updated_ts;
    if (delta > 1) {
      lv_chart_set_next_value(chart, series, v);
      last_updated_ts = std::time(nullptr);
    }
  }
}

void TempCard::handle_edit(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    numpad.set_callback([this](double v) {
      ws.gcode_script(fmt::format("SET_HEATER_TEMPERATURE HEATER={} TARGET={}", id, v));
    });
    numpad.foreground_reset();
  }
}
