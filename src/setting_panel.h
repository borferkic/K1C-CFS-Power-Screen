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
  PrinterSelectPanel printer_select_panel;
  SquareButton wifi_btn;
  SquareButton restart_klipper_btn;
  SquareButton restart_firmware_btn;
  SquareButton sysinfo_btn;
  SquareButton spoolman_btn;
  SquareButton powerscreen_restart_btn;
  SquareButton powerscreen_update_btn;
  SquareButton printer_select_btn;

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

