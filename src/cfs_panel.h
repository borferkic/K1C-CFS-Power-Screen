#ifndef __CFS_PANEL_H__
#define __CFS_PANEL_H__

#include "cfs_model.h"
#include "websocket_client.h"
#include "lvgl/lvgl.h"

#include <mutex>

namespace cfs_ui {

// Spool drawn as a colored disc with a hole (design px). `hole_bg` is the color of the surface behind it, `number`
// (1..4) is written in the hole (0 = no number).
lv_obj_t *spool_create(lv_obj_t *parent, int size, lv_color_t hole_bg, int number);

// Paints a spool for a slot: empty = outline only, not set = neutral grey, defined = its color. `active` adds the
// green ring.
void spool_set(lv_obj_t *spool, const cfs::Slot &slot, bool active);

}  // namespace cfs_ui

// CFS screen: the four slots of the box and the details of the selected one (read-only).
class CfsPanel {
 public:
  CfsPanel(KWebSocketClient &c, std::mutex &l);
  ~CfsPanel();

  // Called with the LVGL lock held whenever the box data changes.
  void update(const cfs::State &state);
  void foreground();
  void handle_slot_click(int index);

 private:
  void select_slot(int index);
  void refresh();

  struct SlotWidget {
    lv_obj_t *card = NULL;
    lv_obj_t *spool = NULL;
    lv_obj_t *letter = NULL;
    lv_obj_t *name = NULL;
    lv_obj_t *sub = NULL;
  };

  struct SlotHandle {
    CfsPanel *panel;
    int index;
  };

  KWebSocketClient &ws;
  std::mutex &lv_lock;
  lv_obj_t *cont;
  lv_obj_t *slots_card;
  lv_obj_t *detail_card;
  lv_obj_t *empty;
  lv_obj_t *header_label;
  lv_obj_t *version_label;
  SlotWidget slots[4];
  SlotHandle handles[4];
  lv_obj_t *detail_spool;
  lv_obj_t *detail_slot_label;
  lv_obj_t *detail_name;
  lv_obj_t *detail_info;
  lv_obj_t *detail_remain;
  cfs::State state;
  int selected;
  bool user_selected;
};

#endif  // __CFS_PANEL_H__
