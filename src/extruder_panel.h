#ifndef __EXTRUDER_PANEL_H__
#define __EXTRUDER_PANEL_H__

#include "websocket_client.h"
#include "notify_consumer.h"
#include "spoolman_panel.h"
#include "selector.h"
#include "button_container.h"
#include "sensor_container.h"
#include "numpad.h"
#include "lvgl/lvgl.h"

#include <ctime>

class ExtruderPanel : public NotifyConsumer {
 public:
  ExtruderPanel(KWebSocketClient &ws, std::mutex &l, Numpad &np, SpoolmanPanel &sm);
  ~ExtruderPanel();

  void foreground();
  void enable_spoolman();  
  void consume(json &j);
  void handle_callback(lv_event_t *e);
  void update_clock();

  static void _handle_callback(lv_event_t *event) {
    ExtruderPanel *panel = (ExtruderPanel*)event->user_data;
    panel->handle_callback(event);
  };

  static void _update_clock_cb(lv_timer_t *timer) {
    ExtruderPanel *panel = static_cast<ExtruderPanel *>(timer->user_data);
    panel->update_clock();
  }

 private:
  KWebSocketClient &ws;
  lv_obj_t *panel_cont;
  lv_obj_t *title_bar;
  lv_obj_t *title_label;
  lv_obj_t *time_label;
  lv_timer_t *clock_timer;
  SpoolmanPanel &spoolman_panel;
  SensorContainer extruder_temp;
  Selector temp_selector;
  Selector length_selector;
  Selector speed_selector;
  lv_obj_t *rightside_btns_cont;
  lv_obj_t *leftside_btns_cont;
  ButtonContainer load_btn;
  ButtonContainer unload_btn;
  ButtonContainer cooldown_btn;
  ButtonContainer manual_change_btn;
  ButtonContainer spoolman_btn;
  ButtonContainer extrude_btn;
  ButtonContainer retract_btn;
  ButtonContainer back_btn;
  std::string load_filament_macro;
  std::string unload_filament_macro;
  std::string cooldown_macro;

  void show_manual_filament_change();
};

#endif // __EXTRUDER_PANEL_H__
