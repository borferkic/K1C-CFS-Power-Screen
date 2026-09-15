#ifndef __SYSINFO_PANEL_H__
#define __SYSINFO_PANEL_H__

#include "button_container.h"
#include "lvgl/lvgl.h"

#include <vector>
#include <string>

class SysInfoPanel {
 public:
  SysInfoPanel();
  ~SysInfoPanel();

  void foreground();
  void handle_callback(lv_event_t *event);

 static void _handle_callback(lv_event_t *event) {
    SysInfoPanel *panel = (SysInfoPanel*)event->user_data;
    panel->handle_callback(event);
  };

 private:
  lv_obj_t *cont;
  lv_obj_t *title_label;
  lv_obj_t *time_label;
  lv_timer_t *clock_timer;

  lv_obj_t *network_card;
  lv_obj_t *network_title_label;
  lv_obj_t *network_name_label;
  lv_obj_t *network_ip_label;
  lv_obj_t *printer_img;

  lv_obj_t *controls_card;

  lv_obj_t *disp_sleep_cont;
  lv_obj_t *display_sleep_dd;

  lv_obj_t *estop_toggle_cont;
  lv_obj_t *prompt_estop_toggle;

  lv_obj_t *z_icon_toggle_cont;
  lv_obj_t *z_icon_toggle;

  lv_obj_t *ll_cont;
  lv_obj_t *loglevel_dd;
  uint32_t loglevel;

  lv_obj_t *brand_label;
  lv_obj_t *version_label;
  lv_obj_t *update_button;
  lv_obj_t *update_button_label;
  lv_obj_t *update_status;

  ButtonContainer back_btn;

  static std::vector<std::string> log_levels;

  void update_clock();
  void refresh_network();
  void check_for_update();
  void run_update();

  static void _update_clock_cb(lv_timer_t *timer) {
    SysInfoPanel *panel = (SysInfoPanel *)timer->user_data;
    panel->update_clock();
  }
};

#endif //__SYSINFO_PANEL_H__
