#ifndef __PID_TUNE_PANEL_H__
#define __PID_TUNE_PANEL_H__

#include "lvgl/lvgl.h"
#include "websocket_client.h"
#include "notify_consumer.h"
#include "button_container.h"

#include <mutex>
#include <string>
#include <vector>

// PID tune of the hotend and the bed: choose a target temperature, start PID_CALIBRATE and save the result
// (SAVE_CONFIG restarts Klipper). Not available while printing.
class PidTunePanel : public NotifyConsumer {
 public:
  PidTunePanel(KWebSocketClient &c, std::mutex &l);
  ~PidTunePanel();

  void init(json &j);
  void foreground();
  void consume(json &j);
  void handle_callback(lv_event_t *event);

  static void _handle_callback(lv_event_t *event) {
    PidTunePanel *panel = (PidTunePanel*)event->user_data;
    panel->handle_callback(event);
  };

 private:
  struct HeaterCard {
    std::string heater;                 // Klipper object: "extruder" or "heater_bed"
    std::vector<int> options;
    std::vector<std::string> texts;
    std::vector<const char *> map;
    lv_obj_t *current = NULL;
    lv_obj_t *btnm = NULL;
    lv_obj_t *start = NULL;
    lv_obj_t *start_label = NULL;
  };

  void build_card(HeaterCard &card, int x, const char *title, const lv_img_dsc_t *icon_src, lv_color_t icon_color,
                  const std::vector<int> &options, int default_option);
  void set_printing(bool printing);
  void set_tuning(bool tuning);
  void start_tune(HeaterCard &card);
  int selected_target(HeaterCard &card);

  KWebSocketClient &ws;
  lv_obj_t *cont;
  HeaterCard hotend;
  HeaterCard bed_card;
  lv_obj_t *notice_title;
  lv_obj_t *notice_text;
  ButtonContainer back_btn;  // hidden: Back lives in the title bar
  bool printing;
  bool tuning;
};

#endif // __PID_TUNE_PANEL_H__
