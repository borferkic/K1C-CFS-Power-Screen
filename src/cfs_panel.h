#ifndef __CFS_PANEL_H__
#define __CFS_PANEL_H__

#include "cfs_model.h"
#include "websocket_client.h"
#include "lvgl/lvgl.h"

#include <mutex>
#include <string>
#include <vector>

namespace cfs_ui {

// Spool drawn as a colored disc with a hole (design px). `hole_bg` is the color of the surface behind it, `number`
// (1..4) is written in the hole (0 = no number).
lv_obj_t *spool_create(lv_obj_t *parent, int size, lv_color_t hole_bg, int number);

// Paints a spool for a slot: empty = outline only, not set = neutral grey, defined = its color. `active` adds the
// green ring.
void spool_set(lv_obj_t *spool, const cfs::Slot &slot, bool active);

}  // namespace cfs_ui

// CFS screen: the four slots of the box, the details of the selected one and the slot editor.
class CfsPanel {
 public:
  CfsPanel(KWebSocketClient &c, std::mutex &l);
  ~CfsPanel();

  // Called with the LVGL lock held whenever the box data changes.
  void update(const cfs::State &state);
  void foreground();
  void handle_slot_click(int index);

  // Slot editor (material and color of the spool in a slot).
  void open_edit(int index);
  void close_edit();
  void save_edit();
  void handle_edit_choice(int which);  // 0 brand, 1 type, 2 filament
  void pick_color(uint32_t color);
  void open_color_dialog();
  void close_color_dialog(bool apply);
  void handle_color_slider();
  void check_saved();

 private:
  void select_slot(int index);
  void refresh();
  void fill_brands();
  void fill_types();
  void fill_names();
  void update_color_ui();
  void show_status(const std::string &text, uint32_t color, uint32_t clear_after_ms);
  static void status_timer_cb(lv_timer_t *timer);
  static void verify_timer_cb(lv_timer_t *timer);

  struct SlotWidget {
    lv_obj_t *card = NULL;
    lv_obj_t *spool = NULL;
    lv_obj_t *letter = NULL;
    lv_obj_t *name = NULL;
    lv_obj_t *sub = NULL;
    lv_obj_t *pencil = NULL;
  };

  struct SlotHandle {
    CfsPanel *panel;
    int index;
  };

  struct ColorHandle {
    CfsPanel *panel;
    uint32_t color;
  };

  KWebSocketClient &ws;
  std::mutex &lv_lock;
  lv_obj_t *cont;
  lv_obj_t *slots_card;
  lv_obj_t *detail_card;
  lv_obj_t *empty;
  lv_obj_t *header_label;
  lv_obj_t *version_label;
  lv_obj_t *status_label;
  SlotWidget slots[4];
  SlotHandle handles[4];
  lv_obj_t *detail_spool;
  lv_obj_t *detail_slot_label;
  lv_obj_t *detail_name;
  lv_obj_t *detail_info;
  lv_obj_t *detail_remain;
  lv_obj_t *detail_edit_btn;
  cfs::State state;
  int selected;
  bool user_selected;

  // Editor.
  lv_obj_t *edit_root;
  lv_obj_t *edit_title;
  lv_obj_t *brand_dd;
  lv_obj_t *type_dd;
  lv_obj_t *name_dd;
  lv_obj_t *swatches[12];
  lv_obj_t *hex_preview;
  lv_obj_t *hex_label;
  ColorHandle swatch_handles[12];
  SlotHandle choice_handles[3];
  int edit_slot;
  std::string edit_brand;
  std::string edit_type;
  std::string edit_material_id;
  uint32_t edit_color;
  bool color_touched;
  std::vector<std::string> brand_list;
  std::vector<std::string> type_list;
  std::vector<cfs::Material> name_list;

  // Custom color dialog (three sliders).
  lv_obj_t *color_root;
  lv_obj_t *color_preview;
  lv_obj_t *color_hex;
  lv_obj_t *color_sliders[3];
  uint32_t color_draft;

  // Result of the last save, checked once the printer reports the new values.
  struct Pending {
    int slot = -1;
    std::string material_id;
    uint32_t color = 0;
  } pending;
  lv_timer_t *status_timer;
  lv_timer_t *verify_timer;
};

#endif  // __CFS_PANEL_H__
