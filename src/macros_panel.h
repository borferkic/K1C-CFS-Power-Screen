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

// Macros screen (PowerUI): search, "Show hidden" switch, group pills and a 3-column grid of macro tiles. A tile without
// parameters runs with one touch; one with parameters opens a small card with its fields. A long touch opens the
// options of the macro (hide, move to a group). Groups are made by the user: "+" creates one, a long touch on a pill
// renames or deletes it. They are saved in Moonraker's database (namespace powerscreen, key macros.groups).
class MacrosPanel {
 public:
  MacrosPanel(KWebSocketClient &c, std::mutex &l, lv_obj_t *parent);
  ~MacrosPanel();

  // Rebuilds the tiles from the macros Klipper reports.
  void populate();

  void handle_search_event(lv_event_t *e);
  void handle_show_hidden(lv_event_t *e);
  void handle_pill_event(lv_event_t *e);

  static void _handle_search_event(lv_event_t *e) {
    MacrosPanel *panel = (MacrosPanel*)e->user_data;
    panel->handle_search_event(e);
  };

  static void _handle_show_hidden(lv_event_t *e) {
    MacrosPanel *panel = (MacrosPanel*)e->user_data;
    panel->handle_show_hidden(e);
  };

  static void _handle_pill_event(lv_event_t *e) {
    MacrosPanel *panel = (MacrosPanel*)e->user_data;
    panel->handle_pill_event(e);
  };

 private:
  static constexpr int MAX_GROUPS = 6;

  // SAVE_CONFIG and any restart macro always ask for confirmation before they run.
  static bool needs_confirmation(const std::string &name);
  void confirm_run(MacroItem &item);
  void run_or_open(MacroItem &item);
  void apply_filter();
  bool in_selected_group(const std::string &macro) const;
  void open_params(MacroItem &item);
  void open_options(MacroItem &item);
  void set_macro_hidden(const std::string &name, bool hide);

  // Groups.
  void load_groups();
  void save_groups();
  void rebuild_pills();
  void schedule_rebuild_pills();
  std::string validate_group_name(const std::string &name, const std::string &ignore) const;
  void open_move_menu(const std::string &macro);
  void open_new_group(const std::string &assign_macro);
  void open_group_options(const std::string &group);
  void open_rename_group(const std::string &group);
  void confirm_delete_group(const std::string &group);
  void assign_macro(const std::string &macro, const std::string &group);

  KWebSocketClient &ws;
  std::mutex &lv_lock;
  lv_obj_t *cont;
  lv_obj_t *search;
  lv_obj_t *show_hidden_switch;
  lv_obj_t *pill_row;
  lv_obj_t *grid;
  lv_obj_t *kb;
  std::vector<std::shared_ptr<MacroItem>> macro_items;
  // Hidden state kept here so it survives reopening before Moonraker's cached copy is refreshed.
  std::map<std::string, bool> hidden_state;

  bool groups_loaded;
  std::vector<std::string> groups;
  std::map<std::string, std::string> assignment;  // macro -> group
  std::string selected;                          // "" = All, OTHER_ID = Other, else a group name
  std::vector<std::string> pill_ids;             // id of each pill, in the order of the row
};

#endif // __MACROS_PANEL_H__
