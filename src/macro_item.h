#ifndef __MACRO_ITEM_H__
#define __MACRO_ITEM_H__

#include "lvgl/lvgl.h"

#include <functional>
#include <map>
#include <string>

// One tile of the Macros screen: the macro name, how many parameters it takes and a play or settings mark.
// A short touch calls `on_tap`, a long touch calls `on_long_press`.
class MacroItem {
 public:
  using Callback = std::function<void(MacroItem &)>;

  MacroItem(lv_obj_t *parent,
	    const std::string &macro_name,
	    const std::map<std::string, std::string> &macro_params,
	    lv_coord_t tile_width,
	    bool hide,
	    Callback on_tap,
	    Callback on_long_press);

  ~MacroItem();

  const std::string &name() const { return macro_name; }
  const std::map<std::string, std::string> &params() const { return macro_params; }
  bool is_hidden() const { return hidden; }
  void set_hidden(bool hide);
  // Shown when it matches the search and is not hidden (hidden tiles appear dimmed while "Show hidden" is on).
  void update_visibility(bool matches_search, bool show_hidden);

  static void _handle_event(lv_event_t *e) {
    MacroItem *item = (MacroItem*)e->user_data;
    item->handle_event(e);
  };

 private:
  void handle_event(lv_event_t *e);

  lv_obj_t *cont;
  std::string macro_name;
  std::map<std::string, std::string> macro_params;
  bool hidden;
  Callback tap_cb;
  Callback long_press_cb;
};

#endif // __MACRO_ITEM_H__
