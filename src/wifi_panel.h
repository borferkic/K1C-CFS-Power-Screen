#ifndef __WIFI_PANEL_H__
#define __WIFI_PANEL_H__

#include "wpa_event.h"
#include "lvgl/lvgl.h"
#include <mutex>

#include <map>
#include <set>
#include <string>
#include <vector>

// Wi-Fi screen (PowerUI): a card with the networks around (signal bars, lock, connected mark) and a status card
// with the selected network and its actions (Connect, Scan again, Forget).
class WifiPanel {
 public:
  WifiPanel(std::mutex &l);

  ~WifiPanel();

  void foreground();
  void handle_wpa_event(const std::string &events);
  void handle_kb_input(lv_event_t *e);
  void handle_row(lv_event_t *e);
  void handle_action(lv_event_t *e);
  void connect(const char *);
  bool find_current_network();
  void scan();

  static void _handle_row(lv_event_t *e) {
    static_cast<WifiPanel*>(e->user_data)->handle_row(e);
  };

  static void _handle_action(lv_event_t *e) {
    static_cast<WifiPanel*>(e->user_data)->handle_action(e);
  };

  static void _handle_kb_input(lv_event_t *e) {
    static_cast<WifiPanel*>(e->user_data)->handle_kb_input(e);
  };

 private:
  struct Network {
    std::string ssid;
    int signal = -100;   // dBm
    int frequency = 2412;
    std::string flags;
  };

  std::mutex &lv_lock;
  WpaEvent wpa_event;
  lv_obj_t *cont;
  lv_obj_t *spinner;
  lv_obj_t *list_card;
  lv_obj_t *list;
  lv_obj_t *refresh_btn;
  lv_obj_t *status_card;
  lv_obj_t *status_badge;
  lv_obj_t *ssid_label;
  lv_obj_t *ip_label;
  lv_obj_t *signal_bars;
  lv_obj_t *signal_label;
  lv_obj_t *security_label;
  lv_obj_t *connect_btn;
  lv_obj_t *connect_label;
  lv_obj_t *scan_btn;
  lv_obj_t *forget_btn;
  lv_obj_t *password_cont;
  lv_obj_t *password_label;
  lv_obj_t *password_input;
  lv_obj_t *kb;

  std::vector<Network> networks;
  int selected = -1;
  std::string selected_network;
  std::string cur_network;
  std::map<std::string, std::string> list_networks;  // saved networks: ssid -> wpa_supplicant id

  void rebuild_list();
  void update_status();
  void show_password_prompt(bool show);
  bool is_saved(const std::string &ssid) const;
  static bool is_secure(const Network &network);
};

#endif // __WIFI_PANEL_H__
