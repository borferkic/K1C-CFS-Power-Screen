#ifndef __TEMP_CARD_H__
#define __TEMP_CARD_H__

#include "websocket_client.h"
#include "numpad.h"
#include "lvgl/lvgl.h"

#include <ctime>
#include <string>

// PowerUI temperature card for the Home screen: series color bar, icon tile, name, current value and target.
// Also feeds the shared temperature chart. Tapping an editable card opens the numpad to set the target.
class TempCard {
 public:
  TempCard(KWebSocketClient &c,
           lv_obj_t *parent,
           int x, int y, int w, int h,
           const lv_img_dsc_t *icon,
           const char *name,
           lv_color_t color,
           bool editable,
           Numpad &np,
           std::string id,
           lv_obj_t *chart,
           lv_chart_series_t *chart_series);
  ~TempCard();

  void update_target(int new_target);
  void update_value(int new_value);
  void update_series(int value);
  void handle_edit(lv_event_t *event);

  static void _handle_edit(lv_event_t *event) {
    TempCard *card = (TempCard *)event->user_data;
    card->handle_edit(event);
  };

 private:
  KWebSocketClient &ws;
  lv_obj_t *cont;
  lv_obj_t *value_label;
  lv_obj_t *target_label;
  int value;
  int target;
  Numpad &numpad;
  std::string id;
  lv_obj_t *chart;
  lv_chart_series_t *series;
  std::time_t last_updated_ts;
};

#endif // __TEMP_CARD_H__
