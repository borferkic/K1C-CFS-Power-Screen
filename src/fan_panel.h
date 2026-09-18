#ifndef __FAN_PANEL_H__
#define __FAN_PANEL_H__

#include "lvgl/lvgl.h"
#include "websocket_client.h"
#include "notify_consumer.h"
#include "slider_container.h"
#include "button_container.h"

#include <map>
#include <memory>
#include <mutex>
#include <ctime>

class FanPanel : public NotifyConsumer {
 public:
  FanPanel(KWebSocketClient &ws, std::mutex &lock);
  ~FanPanel();

  void consume(json &j);
  
  lv_obj_t *get_container();
  void create_fans(json &f);
  void foreground();
  void handle_callback(lv_event_t *event);
  void update_clock();
  void handle_fan_update(lv_event_t *event);
  void handle_fan_update_part_fan(lv_event_t *event);
  void handle_fan_update_generic(lv_event_t *event);

  static void _handle_callback(lv_event_t *event) {
    FanPanel *panel = (FanPanel*)event->user_data;
    panel->handle_callback(event);
  };

  static void _handle_fan_update(lv_event_t *event) {
    FanPanel *panel = (FanPanel*)event->user_data;
    panel->handle_fan_update(event);
  };

  static void _handle_fan_update_part_fan(lv_event_t *event) {
    FanPanel *panel = (FanPanel*)event->user_data;
    panel->handle_fan_update_part_fan(event);
  };
  
  static void _handle_fan_update_generic(lv_event_t *event) {
    FanPanel *panel = (FanPanel*)event->user_data;
    panel->handle_fan_update_generic(event);
  };

  static void _update_clock_cb(lv_timer_t *timer) {
    FanPanel *panel = static_cast<FanPanel *>(timer->user_data);
    panel->update_clock();
  }

 private:

  KWebSocketClient &ws;
  lv_obj_t *fanpanel_cont;
  lv_obj_t *title_bar;
  lv_obj_t *title_label;
  lv_obj_t *time_label;
  lv_timer_t *clock_timer;
  lv_obj_t *fans_cont;
  std::map<std::string, std::shared_ptr<SliderContainer>> fans;
  /* SliderContainer fan0; */
  /* SliderContainer fan1; */
  /* SliderContainer fan2; */
  ButtonContainer back_btn;
};

#endif // __FAN_PANEL_H__
