#ifndef __BELTS_CALIBRATION_PANEL_H__
#define __BELTS_CALIBRATION_PANEL_H__

#include "websocket_client.h"
#include "button_container.h"
#include "lvgl/lvgl.h"

#include <mutex>
#include <string>
#include <vector>

class BeltsCalibrationPanel {
 public:
  BeltsCalibrationPanel(KWebSocketClient &c, std::mutex &l);
  ~BeltsCalibrationPanel();

  void foreground();
  void handle_callback(lv_event_t *event);
  void handle_image_clicked(lv_event_t *event);
  void handle_macro_response(json &j);
  void handle_update_slider(lv_event_t *event);

  static void _handle_callback(lv_event_t *event) {
    BeltsCalibrationPanel *panel = (BeltsCalibrationPanel*)event->user_data;
    panel->handle_callback(event);
  };
  
  static void _handle_image_clicked(lv_event_t *event) {
    BeltsCalibrationPanel *panel = (BeltsCalibrationPanel*)event->user_data;
    panel->handle_image_clicked(event);
  };

  static void _handle_update_slider(lv_event_t *event) {
    BeltsCalibrationPanel *panel = (BeltsCalibrationPanel*)event->user_data;
    panel->handle_update_slider(event);
  };

 private:
  KWebSocketClient &ws;
  std::mutex &lv_lock;
  lv_obj_t *cont;
  lv_obj_t *graph_cont;
  lv_obj_t *graph;
  lv_obj_t *spinner;
  lv_obj_t *excite_control;
  lv_obj_t *excite_slider;
  lv_obj_t *excite_label;
  lv_obj_t *excite_dd;
  lv_obj_t *graph_hint;     // placeholder while there is no graph
  lv_obj_t *calibrate_btn;
  lv_obj_t *excite_btn;
  lv_obj_t *stop_btn;
  ButtonContainer emergency_btn;  // hidden: keeps the "Do you want to emergency stop?" confirmation
  ButtonContainer back_btn;       // hidden: Back lives in the title bar
  bool image_fullsized;

  void fit_graph();
  void emergency_stop();

  static std::vector<std::string> axes;

};

#endif // __BELTS_CALIBRATION_PANEL_H__
