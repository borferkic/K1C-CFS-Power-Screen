#include "cfs_panel.h"
#include "powerui.h"
#include "spdlog/spdlog.h"

#include <string>

LV_IMG_DECLARE(ui_cfs_img);

using namespace powerui;

namespace cfs_ui {

lv_obj_t *spool_create(lv_obj_t *parent, int size, lv_color_t hole_bg, int number) {
  lv_obj_t *spool = lv_obj_create(parent);
  lv_obj_remove_style_all(spool);
  lv_obj_clear_flag(spool, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(spool, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(spool, px(size), px(size));
  lv_obj_set_style_radius(spool, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(spool, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(spool, px(2), 0);
  lv_obj_set_style_border_opa(spool, LV_OPA_TRANSP, 0);
  lv_obj_set_style_outline_width(spool, px(2), 0);
  lv_obj_set_style_outline_pad(spool, px(2), 0);
  lv_obj_set_style_outline_color(spool, lv_color_hex(COLOR_ACCENT), 0);
  lv_obj_set_style_outline_opa(spool, LV_OPA_TRANSP, 0);

  lv_obj_t *hole = lv_obj_create(spool);
  lv_obj_remove_style_all(hole);
  lv_obj_clear_flag(hole, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(hole, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(hole, px(size / 2), px(size / 2));
  lv_obj_set_style_radius(hole, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(hole, hole_bg, 0);
  lv_obj_set_style_bg_opa(hole, LV_OPA_COVER, 0);
  lv_obj_center(hole);

  if (number > 0) {
    lv_obj_t *n = label(hole, std::to_string(number).c_str(), size >= 36 ? &lv_font_montserrat_14 : &lv_font_montserrat_12,
                        lv_color_hex(COLOR_FG));
    lv_obj_center(n);
  }
  return spool;
}

void spool_set(lv_obj_t *spool, const cfs::Slot &slot, bool active) {
  if (spool == NULL) {
    return;
  }
  switch (slot.kind) {
    case cfs::SlotKind::Empty:
      lv_obj_set_style_bg_opa(spool, LV_OPA_TRANSP, 0);
      lv_obj_set_style_border_color(spool, lv_color_hex(COLOR_NEUTRAL), 0);
      lv_obj_set_style_border_opa(spool, LV_OPA_COVER, 0);
      break;
    case cfs::SlotKind::Unset:
    case cfs::SlotKind::Defined:
      lv_obj_set_style_bg_opa(spool, LV_OPA_COVER, 0);
      lv_obj_set_style_bg_color(spool, lv_color_hex(slot.has_color ? slot.color : COLOR_NEUTRAL), 0);
      lv_obj_set_style_border_opa(spool, LV_OPA_TRANSP, 0);
      break;
  }
  lv_obj_set_style_outline_opa(spool, active ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
}

}  // namespace cfs_ui

namespace {

constexpr int SLOT_W = 162;
constexpr int SLOT_H = 144;
constexpr int SLOT_GAP = 10;

std::string slot_letter(int i) { return std::string(1, static_cast<char>('A' + i)); }

std::string slot_subtitle(const cfs::Slot &s) {
  switch (s.kind) {
    case cfs::SlotKind::Empty:
      return "No spool";
    case cfs::SlotKind::Unset:
      return "Spool loaded";
    case cfs::SlotKind::Defined: {
      std::string text = s.material;
      if (s.remain_len >= 0) {
        text += (text.empty() ? "" : " · ") + std::to_string(s.remain_len) + " m";
      }
      return text.empty() ? std::string("Spool loaded") : text;
    }
  }
  return std::string();
}

}  // namespace

CfsPanel::CfsPanel(KWebSocketClient &c, std::mutex &l)
  : ws(c)
  , lv_lock(l)
  , cont(lv_obj_create(lv_scr_act()))
  , slots_card(NULL)
  , detail_card(NULL)
  , empty(NULL)
  , header_label(NULL)
  , version_label(NULL)
  , detail_spool(NULL)
  , detail_slot_label(NULL)
  , detail_name(NULL)
  , detail_info(NULL)
  , detail_remain(NULL)
  , selected(0)
  , user_selected(false)
{
  style_overlay_root(cont);
  lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
  lv_obj_add_flag(cont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_background(cont);

  // ---- Slots card.
  slots_card = card(cont, 12, 12, 712, 208);
  header_label = label(slots_card, "CFS · 4 SLOTS", &lv_font_montserrat_14, lv_color_hex(COLOR_MUTED));
  lv_obj_set_pos(header_label, px(20), px(16));
  version_label = label(slots_card, "", &lv_font_montserrat_12, lv_color_hex(COLOR_MUTED));
  lv_obj_align(version_label, LV_ALIGN_TOP_RIGHT, -px(20), px(18));

  for (int i = 0; i < 4; ++i) {
    SlotWidget &w = slots[i];
    handles[i] = {this, i};
    w.card = card(slots_card, 16 + i * (SLOT_W + SLOT_GAP), 48, SLOT_W, SLOT_H);
    lv_obj_set_style_bg_color(w.card, lv_color_hex(COLOR_BG), 0);
    lv_obj_add_flag(w.card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(w.card, [](lv_event_t *e) {
      SlotHandle *h = static_cast<SlotHandle *>(e->user_data);
      h->panel->handle_slot_click(h->index);
    }, LV_EVENT_CLICKED, &handles[i]);

    w.letter = label(w.card, slot_letter(i).c_str(), &lv_font_montserrat_12, lv_color_hex(COLOR_MUTED));
    lv_obj_set_pos(w.letter, px(12), px(8));

    w.spool = cfs_ui::spool_create(w.card, 56, lv_color_hex(COLOR_BG), 0);
    lv_obj_align(w.spool, LV_ALIGN_TOP_MID, 0, px(22));

    w.name = label(w.card, "", &lv_font_montserrat_14, lv_color_hex(COLOR_FG));
    lv_obj_set_width(w.name, px(SLOT_W - 16));
    lv_obj_set_style_text_align(w.name, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(w.name, LV_LABEL_LONG_DOT);
    lv_obj_align(w.name, LV_ALIGN_TOP_MID, 0, px(92));

    w.sub = label(w.card, "", &lv_font_montserrat_12, lv_color_hex(COLOR_MUTED));
    lv_obj_set_width(w.sub, px(SLOT_W - 16));
    lv_obj_set_style_text_align(w.sub, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(w.sub, LV_LABEL_LONG_DOT);
    lv_obj_align(w.sub, LV_ALIGN_TOP_MID, 0, px(114));
  }

  // ---- Detail card of the selected slot.
  detail_card = card(cont, 12, 232, 712, 196);
  detail_spool = cfs_ui::spool_create(detail_card, 84, lv_color_hex(COLOR_CARD), 0);
  lv_obj_set_pos(detail_spool, px(36), px(56));
  detail_slot_label = label(detail_card, "", &lv_font_montserrat_14, lv_color_hex(COLOR_MUTED));
  lv_obj_set_pos(detail_slot_label, px(152), px(40));
  detail_name = label(detail_card, "", &lv_font_montserrat_24, lv_color_hex(COLOR_FG));
  lv_obj_set_width(detail_name, px(520));
  lv_label_set_long_mode(detail_name, LV_LABEL_LONG_DOT);
  lv_obj_set_pos(detail_name, px(152), px(64));
  detail_info = label(detail_card, "", &lv_font_montserrat_14, lv_color_hex(COLOR_MUTED));
  lv_obj_set_width(detail_info, px(520));
  lv_obj_set_pos(detail_info, px(152), px(104));
  detail_remain = label(detail_card, "", &lv_font_montserrat_14, lv_color_hex(COLOR_MUTED));
  lv_obj_set_width(detail_remain, px(520));
  lv_obj_set_pos(detail_remain, px(152), px(130));

  // ---- Shown instead of both cards while the CFS is not connected.
  empty = empty_state(cont, &ui_cfs_img, "CFS not connected",
                      "Connect the CFS to the printer to see its four slots here.");
  lv_obj_add_flag(empty, LV_OBJ_FLAG_HIDDEN);

  refresh();
}

CfsPanel::~CfsPanel() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

void CfsPanel::update(const cfs::State &s) {
  const bool was_connected = state.connected;
  state = s;
  if (!state.connected) {
    user_selected = false;
  } else if (!was_connected) {
    user_selected = false;
  }
  refresh();
}

void CfsPanel::foreground() {
  lv_obj_clear_flag(cont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(cont);
  powerui::overlay_open("CFS", [this]() {
    lv_obj_add_flag(cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_background(cont);
  });
}

void CfsPanel::handle_slot_click(int index) {
  user_selected = true;
  select_slot(index);
}

void CfsPanel::select_slot(int index) {
  if (index < 0 || index > 3) {
    return;
  }
  selected = index;
  refresh();
}

void CfsPanel::refresh() {
  if (cont == NULL) {
    return;
  }
  const bool connected = state.connected;
  if (connected) {
    lv_obj_clear_flag(slots_card, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(detail_card, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(empty, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(slots_card, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(detail_card, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(empty, LV_OBJ_FLAG_HIDDEN);
    return;
  }

  // Default selection: the active slot, else the first slot that holds a spool.
  if (!user_selected) {
    int pick = state.active;
    for (int i = 0; pick < 0 && i < 4; ++i) {
      if (state.slots[i].kind != cfs::SlotKind::Empty) {
        pick = i;
      }
    }
    selected = pick >= 0 ? pick : 0;
  }

  std::string version = "Box " + std::to_string(state.box);
  if (!state.version.empty()) {
    version += " · v" + state.version;
  }
  lv_label_set_text(version_label, version.c_str());

  for (int i = 0; i < 4; ++i) {
    const cfs::Slot &s = state.slots[i];
    SlotWidget &w = slots[i];
    cfs_ui::spool_set(w.spool, s, state.active == i);
    lv_label_set_text(w.name, s.name.c_str());
    lv_obj_set_style_text_color(w.name, lv_color_hex(s.kind == cfs::SlotKind::Empty ? COLOR_MUTED : COLOR_FG), 0);
    lv_label_set_text(w.sub, slot_subtitle(s).c_str());
    const bool sel = i == selected;
    lv_obj_set_style_border_color(w.card, lv_color_hex(sel ? COLOR_ACCENT : COLOR_WHITE), 0);
    lv_obj_set_style_border_opa(w.card, sel ? LV_OPA_60 : LV_OPA_10, 0);
    lv_obj_set_style_bg_color(w.card, lv_color_hex(sel ? COLOR_ACCENT_BG : COLOR_BG), 0);
  }

  const cfs::Slot &sel = state.slots[selected];
  cfs_ui::spool_set(detail_spool, sel, state.active == selected);
  lv_label_set_text(detail_slot_label, ("Slot " + slot_letter(selected)).c_str());
  lv_label_set_text(detail_name, sel.name.c_str());

  std::string info;
  switch (sel.kind) {
    case cfs::SlotKind::Empty:
      info = "No spool in this slot";
      break;
    case cfs::SlotKind::Unset:
      info = "A spool is loaded. Its material and color are not set yet.";
      break;
    case cfs::SlotKind::Defined:
      info = sel.brand;
      if (!sel.material.empty()) {
        info += (info.empty() ? "" : " · ") + sel.material;
      }
      break;
  }
  lv_label_set_text(detail_info, info.c_str());
  lv_label_set_text(detail_remain, sel.remain_len >= 0 ? (std::to_string(sel.remain_len) + " m remaining").c_str() : "");
}
