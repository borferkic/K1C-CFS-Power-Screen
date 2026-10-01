#ifndef __MAIN_PANEL_H__
#define __MAIN_PANEL_H__

#include "websocket_client.h"
#include "notify_consumer.h"
#include "temp_card.h"
#include "status_icons.h"
#include "button_container.h"
#include "square_button.h"
#include "wide_button.h"
#include "prompt_panel.h"
#include "numpad.h"
#include "homing_panel.h"
#include "extruder_panel.h"
#include "fan_panel.h"
#include "led_panel.h"
#include "print_panel.h"
#include "console_panel.h"
#include "printertune_panel.h"
#include "setting_panel.h"
#include "print_status_panel.h"
#include "spoolman_panel.h"
#include "lvgl/lvgl.h"

#include <functional>
#include <mutex>
#include <string>
#include <vector>
#include <map>
#include <memory>

class MainPanel : public NotifyConsumer {
 public:
  MainPanel(KWebSocketClient &ws,
	    std::mutex &lv_lock,
	    SpoolmanPanel &sm);

  ~MainPanel();
  void consume(json &data);
  void init(json &data);
  void subscribe();
  // Copies the PowerScreen Klipper macros of the installed package to the printer config and restarts Klipper
  // when they changed (never while printing), so every update path ends with the macros the UI needs.
  void sync_klipper_macros(json &printer_status);
  PrinterTunePanel& get_tune_panel();
  void enable_spoolman();
  // The file picker is not reachable from the Home screen anymore; kept for the screen that will host it.
  void open_files();
  void open_console();

  void create_panel();
  void create_sensors(json &temp_sensors);
  void create_fans(json &temp_fans);
  void create_leds(json &leds);
  void handle_homing_cb(lv_event_t *event);
  void handle_extrude_cb(lv_event_t *event);
  void handle_fanpanel_cb(lv_event_t *event);
  void handle_ledpanel_cb(lv_event_t *event);
  void handle_tab_change_cb(lv_event_t *event);
  void handle_view_toggle_cb(lv_event_t *event);
  void handle_tab_click_cb(lv_event_t *event);
  void handle_back_click_cb(lv_event_t *event);

  static void _handle_homing_cb(lv_event_t *event) {
    MainPanel *panel = (MainPanel*)event->user_data;
    panel->handle_homing_cb(event);
  };

  static void _handle_extrude_cb(lv_event_t *event) {
    MainPanel *panel = (MainPanel*)event->user_data;
    panel->handle_extrude_cb(event);
  };

  static void _handle_fanpanel_cb(lv_event_t *event) {
    MainPanel *panel = (MainPanel*)event->user_data;
    panel->handle_fanpanel_cb(event);
  };

  static void _handle_ledpanel_cb(lv_event_t *event) {
    MainPanel *panel = (MainPanel*)event->user_data;
    panel->handle_ledpanel_cb(event);
  };

  static void _handle_tab_change_cb(lv_event_t *event) {
    MainPanel *panel = (MainPanel*)event->user_data;
    panel->handle_tab_change_cb(event);
  };

  static void _handle_back_click_cb(lv_event_t *event) {
    MainPanel *panel = (MainPanel*)event->user_data;
    panel->handle_back_click_cb(event);
  };

  static void _handle_tab_click_cb(lv_event_t *event) {
    MainPanel *panel = (MainPanel*)event->user_data;
    panel->handle_tab_click_cb(event);
  };

  static void _handle_view_toggle_cb(lv_event_t *event) {
    MainPanel *panel = (MainPanel*)event->user_data;
    panel->handle_view_toggle_cb(event);
  };

  private:
  // Quick action button of the Home screen (icon above a label).
  struct QuickButton {
    lv_obj_t *btn = NULL;
    lv_obj_t *icon = NULL;
    lv_obj_t *label = NULL;
  };

  void create_main(lv_obj_t *parent);
  QuickButton create_quick_button(lv_obj_t *parent, int x, int y, int w, int h,
				  const lv_img_dsc_t *icon, const char *text, lv_event_cb_t cb);
  void set_quick_active(QuickButton &button, bool active, const char *text);
  void create_chart_card(lv_obj_t *parent);
  void set_home_view(bool print_view);
  void update_header();
  void set_title(const char *text);
  void push_overlay(const std::string &title, std::function<void()> back);
  void update_nav_indicator();
  void update_clock();
  void update_filament_state(json &root, const std::string &prefix);
  void poll_network();
  static void _update_clock_cb(lv_timer_t *timer) {
    MainPanel *panel = static_cast<MainPanel *>(timer->user_data);
    panel->update_clock();
  }
  static void _poll_network_cb(lv_timer_t *timer) {
    MainPanel *panel = static_cast<MainPanel *>(timer->user_data);
    panel->poll_network();
  }
  KWebSocketClient &ws;
  HomingPanel homing_panel;
  FanPanel fan_panel;
  LedPanel led_panel;
  lv_obj_t *tabview;
  lv_obj_t *nav_highlight;
  lv_obj_t *nav_icons[4];
  lv_coord_t nav_tile_top[4];
  lv_coord_t nav_tile_x;
  lv_obj_t *main_tab;
  lv_obj_t *printertune_tab;
  lv_obj_t *files_tab;
  lv_obj_t *console_page;
  ConsolePanel console_panel;
  lv_obj_t *setting_tab;
  SettingPanel setting_panel;
  lv_obj_t *title_bar;
  lv_obj_t *title_label;
  lv_obj_t *time_label;
  lv_obj_t *logo;
  lv_obj_t *title_label_bold;
  lv_obj_t *back_pill;
  struct Overlay {
    std::string title;
    std::function<void()> back;
  };
  std::vector<Overlay> overlays;
  lv_timer_t *clock_timer;
  lv_timer_t *network_timer;
  std::unique_ptr<StatusIcons> status_icons;
  std::map<std::string, bool> filament_state;
  lv_obj_t *main_cont;
  PrintStatusPanel print_status_panel;
  PrintPanel print_panel;
  PrinterTunePanel printertune_panel;
  Numpad numpad;
  ExtruderPanel extruder_panel;
  PromptPanel prompt_panel;
  SpoolmanPanel &spoolman_panel;

  lv_obj_t *chart_card;
  lv_obj_t *chart_toggle;
  lv_obj_t *chart_badge;
  lv_obj_t *temp_chart;
  bool print_view;

  std::map<std::string, std::shared_ptr<TempCard>> sensors;

  QuickButton homing_btn;
  QuickButton extrude_btn;
  QuickButton action_btn;
  QuickButton led_btn;
};
#endif // __MAIN_PANEL_H__
