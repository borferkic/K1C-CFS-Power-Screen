#ifndef __SETTING_PANEL_H__
#define __SETTING_PANEL_H__

#include "platform.h"

#ifndef OS_ANDROID
#include "wifi_panel.h"
#endif

#include "sysinfo_panel.h"
#include "spoolman_panel.h"
#include "printer_select_panel.h"
#include "button_container.h"
#include "square_button.h"
#include "websocket_client.h"
#include "lvgl/lvgl.h"

#include <functional>
#include <mutex>

class SettingPanel {
 public:
  SettingPanel(KWebSocketClient &c, std::mutex &l, lv_obj_t *parent, SpoolmanPanel &sm);
  ~SettingPanel();

  lv_obj_t *get_container();
  void enable_spoolman();
  // The "CFS" tile opens the CFS screen while the CFS is connected, else Spoolman when it is available.
  void set_cfs_available(bool available);
  void set_cfs_opener(std::function<void()> opener);
  // The "Macros" tile opens the full-screen Macros page owned by the main panel.
  void set_macros_opener(std::function<void()> opener);

  void handle_callback(lv_event_t *event);

  static void _handle_callback(lv_event_t *event) {
    SettingPanel *panel = (SettingPanel*)event->user_data;
    panel->handle_callback(event);
  };

 private:
  KWebSocketClient &ws;
  lv_obj_t *cont;

#ifndef OS_ANDROID
  WifiPanel wifi_panel;
#endif

  SysInfoPanel sysinfo_panel;
  SpoolmanPanel &spoolman_panel;
  bool cfs_available = false;
  bool spoolman_available = false;
  std::function<void()> open_cfs;
  std::function<void()> open_macros;
  void refresh_cfs_button();
  PrinterSelectPanel printer_select_panel;
  SquareButton wifi_btn;
  SquareButton restart_btn;
  SquareButton macros_btn;
  SquareButton sysinfo_btn;
  SquareButton spoolman_btn;
  SquareButton powerscreen_update_btn;
  SquareButton printer_select_btn;

  // The Restart tile opens a list: PowerScreen, Klipper or the firmware.
  void show_restart_menu();
  void restart_powerscreen();
  // Confirmation before restarting Klipper or the firmware.

  // Power Update is disabled while a print is running or paused (like CFS when it is offline).
  lv_timer_t *update_lock_timer;
  bool update_locked;
  void refresh_update_lock();
  static void _refresh_update_lock_cb(lv_timer_t *timer) {
    static_cast<SettingPanel*>(timer->user_data)->refresh_update_lock();
  }

  void show_confirm(const std::string &title, const std::string &method);
};

#endif // __SETTING_PANEL_H__

