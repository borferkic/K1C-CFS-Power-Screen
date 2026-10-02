#ifndef __LIMITS_PANEL_H__
#define __LIMITS_PANEL_H__

#include "button_container.h"
#include "lvgl/lvgl.h"
#include "websocket_client.h"
#include "notify_consumer.h"

#include <mutex>
#include <string>

class LimitsPanel : public NotifyConsumer {
 public:
  LimitsPanel(KWebSocketClient &c, std::mutex &l);
  ~LimitsPanel();

  void init(json &j);
  void foreground();
  void consume(json &j);

  void handle_callback(lv_event_t *event);
  
  static void _handle_callback(lv_event_t *event) {
    LimitsPanel *panel = (LimitsPanel*)event->user_data;
    panel->handle_callback(event);
  };

 private:
  KWebSocketClient &ws;
  lv_obj_t *cont;
  lv_obj_t *limit_cont;
  // One card per limit: name, Reset, big value with unit, slider with its range and the default value.
  struct LimitCard {
    lv_obj_t *card = NULL;
    lv_obj_t *slider = NULL;
    lv_obj_t *value = NULL;
    lv_obj_t *min_label = NULL;
    lv_obj_t *max_label = NULL;
    lv_obj_t *default_label = NULL;
    lv_obj_t *reset = NULL;
    std::string unit;

    lv_obj_t *get_slider() { return slider; }
    lv_obj_t *get_off() { return reset; }
    void set_range(int min_range, int max_range);
    void set_default(int v);
    void update_value(int v);
  };
  LimitCard create_card(int x, int y, const char *name, const char *unit, int max_range);
  static void _handle_value_changed(lv_event_t *event);

  LimitCard velocity;
  LimitCard acceleration;
  LimitCard square_corner;
  LimitCard accel_to_decel;
  ButtonContainer back_btn;
  int max_velocity_default;
  int max_accel_default;
  int max_accel_to_decel_default;
  int square_corner_default;
  
};

#endif // __LIMITS_PANEL_H__
