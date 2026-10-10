#ifndef __MACROS_PANEL_H__
#define __MACROS_PANEL_H__

#include "websocket_client.h"
#include "macro_item.h"
#include "lvgl/lvgl.h"

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

// Macros screen (PowerUI): search, "Show hidden" switch and a 3-column grid of macro tiles. A tile without
// parameters runs with one touch; one with parameters opens a small card with its fields. A long touch opens
// the Hide / Show menu.
class MacrosPanel {
 public:
  MacrosPanel(KWebSocketClient &c, std::mutex &l, lv_obj_t *parent);
  ~MacrosPanel();

  // Rebuilds the tiles from the macros Klipper reports.
  void populate();

  void handle_search_event(lv_event_t *e);
  void handle_show_hidden(lv_event_t *e);

  static void _handle_search_event(lv_event_t *e) {
    MacrosPanel *panel = (MacrosPanel*)e->user_data;
    panel->handle_search_event(e);
  };

  static void _handle_show_hidden(lv_event_t *e) {
    MacrosPanel *panel = (MacrosPanel*)e->user_data;
    panel->handle_show_hidden(e);
  };

 private:
  // SAVE_CONFIG and any restart macro always ask for confirmation before they run.
  static bool needs_confirmation(const std::string &name);
  void confirm_run(MacroItem &item);
  void run_or_open(MacroItem &item);
  void apply_filter();
  void open_params(MacroItem &item);
  void open_options(MacroItem &item);
  void set_macro_hidden(const std::string &name, bool hide);

  KWebSocketClient &ws;
  std::mutex &lv_lock;
  lv_obj_t *cont;
  lv_obj_t *search;
  lv_obj_t *show_hidden_switch;
  lv_obj_t *grid;
  lv_obj_t *kb;
  std::vector<std::shared_ptr<MacroItem>> macro_items;
  // Hidden state kept here so it survives reopening before Moonraker's cached copy is refreshed.
  std::map<std::string, bool> hidden_state;
};

#endif // __MACROS_PANEL_H__
