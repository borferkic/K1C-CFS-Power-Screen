#ifndef __SYSINFO_PANEL_H__
#define __SYSINFO_PANEL_H__

#include "button_container.h"
#include "lvgl/lvgl.h"

#include <atomic>
#include <vector>
#include <string>

class SysInfoPanel {
 public:
  SysInfoPanel();
  ~SysInfoPanel();

  void foreground();
  void handle_callback(lv_event_t *event);
  // Start the update in the background and show the waiting screen.
  void start_update();

 static void _handle_callback(lv_event_t *event) {
    SysInfoPanel *panel = (SysInfoPanel*)event->user_data;
    panel->handle_callback(event);
  };

 private:
  lv_obj_t *cont;
  lv_obj_t *title_bar;
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

  lv_obj_t *channel_cont;
  lv_obj_t *channel_dd;

  lv_obj_t *brand_label;
  lv_obj_t *version_label;
  lv_obj_t *update_button;
  lv_obj_t *update_button_label;
  lv_obj_t *update_status;

  ButtonContainer back_btn;

  // General / Updates tabs (created in the constructor body).
  lv_obj_t *tab_general_btn;
  lv_obj_t *tab_updates_btn;
  lv_obj_t *general_page;
  lv_obj_t *updates_page;
  lv_obj_t *updates_version_label;

  // Update waiting screen (on lv_layer_top).
  lv_obj_t *update_overlay;
  lv_obj_t *update_spinner;
  lv_obj_t *update_title;
  lv_obj_t *update_phase;
  lv_obj_t *update_close_btn;
  lv_timer_t *update_timer;

  // State shared with worker threads. Workers never touch LVGL: they only
  // write here and the timer (LVGL thread) applies the changes.
  std::atomic_bool check_running;
  std::atomic_bool check_done;
  // Last check result: 0 no data, 1 update available,
  // 2 up to date, 3 error.
  std::atomic_int check_result;
  std::atomic_bool update_running;
  std::atomic_bool update_finished;
  std::atomic_int update_exit_code;

  static std::vector<std::string> log_levels;

  void update_clock();
  void refresh_network();
  void check_for_update();
  void create_update_overlay();
  void create_tabs();
  void show_tab(bool updates);
  void show_update_overlay(const std::string &title, const std::string &phase, bool busy);
  void poll_update();
  bool is_printing();
  void show_updated_notice();

  static void _update_clock_cb(lv_timer_t *timer) {
    SysInfoPanel *panel = (SysInfoPanel *)timer->user_data;
    panel->update_clock();
  }

  static void _poll_update_cb(lv_timer_t *timer) {
    SysInfoPanel *panel = (SysInfoPanel *)timer->user_data;
    panel->poll_update();
  }
};

#endif //__SYSINFO_PANEL_H__
