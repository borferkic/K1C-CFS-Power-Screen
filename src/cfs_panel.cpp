#include "cfs_panel.h"
#include "powerui.h"
#include "state.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cstdio>
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

// Palette of the editor (same hues as the PowerUI sketch).
const uint32_t PALETTE[12] = {0xFFFFFF, 0x2B2B2B, 0xEF4444, 0xF97316, 0xEAB308, 0x22C55E,
                              0x06B6D4, 0x3B82F6, 0x8B5CF6, 0xEC4899, 0x9CA3AF, 0xA16207};

std::string slot_letter(int i) { return std::string(1, static_cast<char>('A' + i)); }

std::string hex_text(uint32_t color) {
  char buf[16];
  std::snprintf(buf, sizeof(buf), "#%06X", static_cast<unsigned>(color & 0xFFFFFF));
  return std::string(buf);
}

// Only ASCII: the display fonts have no other characters.
std::string slot_subtitle(const cfs::Slot &s) {
  switch (s.kind) {
    case cfs::SlotKind::Empty:
      return "No spool";
    case cfs::SlotKind::Unset:
      return "Spool loaded";
    case cfs::SlotKind::Defined: {
      std::string text = s.material;
      if (s.remain_len >= 0) {
        text += (text.empty() ? "" : ", ") + std::to_string(s.remain_len) + " m";
      }
      return text.empty() ? std::string("Spool loaded") : text;
    }
  }
  return std::string();
}

lv_obj_t *dim_overlay(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_set_size(o, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(o, lv_color_hex(COLOR_BLACK), 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_60, 0);
  lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
  return o;
}

bool printing_now() {
  auto &pstate = State::get_instance()->get_data(json::json_pointer("/printer_state/print_stats/state"));
  if (pstate.is_string()) {
    const std::string s = pstate.template get<std::string>();
    return s == "printing" || s == "paused";
  }
  return false;
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
  , status_label(NULL)
  , detail_spool(NULL)
  , detail_slot_label(NULL)
  , detail_name(NULL)
  , detail_info(NULL)
  , detail_remain(NULL)
  , detail_edit_btn(NULL)
  , selected(0)
  , user_selected(false)
  , edit_root(NULL)
  , edit_title(NULL)
  , brand_dd(NULL)
  , type_dd(NULL)
  , name_dd(NULL)
  , swatches{NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL}
  , hex_preview(NULL)
  , hex_label(NULL)
  , edit_slot(-1)
  , edit_color(0xFFFFFF)
  , color_touched(false)
  , color_root(NULL)
  , color_preview(NULL)
  , color_hex(NULL)
  , color_sliders{NULL, NULL, NULL}
  , color_draft(0xFFFFFF)
  , status_timer(NULL)
  , verify_timer(NULL)
{
  style_overlay_root(cont);
  lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
  lv_obj_add_flag(cont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_background(cont);

  // ---- Slots card.
  slots_card = card(cont, 12, 12, 712, 208);
  header_label = label(slots_card, "CFS", &lv_font_montserrat_14, lv_color_hex(COLOR_MUTED));
  lv_obj_set_pos(header_label, px(20), px(16));
  status_label = label(slots_card, "", &lv_font_montserrat_12, lv_color_hex(COLOR_MUTED));
  lv_obj_set_pos(status_label, px(76), px(18));
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

    // Pencil: opens the editor of this slot.
    w.pencil = plain(w.card);
    lv_obj_set_size(w.pencil, px(34), px(30));
    lv_obj_set_pos(w.pencil, px(SLOT_W - 38), px(2));
    lv_obj_add_flag(w.pencil, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *pencil_icon = label(w.pencil, LV_SYMBOL_EDIT, &lv_font_montserrat_14, lv_color_hex(COLOR_MUTED));
    lv_obj_center(pencil_icon);
    lv_obj_add_event_cb(w.pencil, [](lv_event_t *e) {
      SlotHandle *h = static_cast<SlotHandle *>(e->user_data);
      h->panel->open_edit(h->index);
    }, LV_EVENT_CLICKED, &handles[i]);

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
  lv_obj_set_width(detail_name, px(330));
  lv_label_set_long_mode(detail_name, LV_LABEL_LONG_DOT);
  lv_obj_set_pos(detail_name, px(152), px(64));
  detail_info = label(detail_card, "", &lv_font_montserrat_14, lv_color_hex(COLOR_MUTED));
  lv_obj_set_width(detail_info, px(340));
  lv_obj_set_pos(detail_info, px(152), px(104));
  detail_remain = label(detail_card, "", &lv_font_montserrat_14, lv_color_hex(COLOR_MUTED));
  lv_obj_set_width(detail_remain, px(340));
  lv_obj_set_pos(detail_remain, px(152), px(130));
  detail_edit_btn = action_button(detail_card, NULL, "Edit slot", ActionKind::Outline, 520, 70, 168, 48,
                                  [](lv_event_t *e) {
                                    CfsPanel *p = static_cast<CfsPanel *>(e->user_data);
                                    p->open_edit(p->selected);
                                  }, this);

  // ---- Shown instead of both cards while the CFS is not connected.
  empty = empty_state(cont, &ui_cfs_img, "CFS not connected",
                      "Connect the CFS to the printer to see its four slots here.");
  lv_obj_add_flag(empty, LV_OBJ_FLAG_HIDDEN);

  // ---- Slot editor (modal over the screen).
  edit_root = dim_overlay(cont);
  lv_obj_t *ec = card(edit_root, 24, 12, 688, 416);
  edit_title = label(ec, "Edit slot", &lv_font_montserrat_20, lv_color_hex(COLOR_FG));
  lv_obj_set_pos(edit_title, px(24), px(16));
  lv_obj_t *edit_sub = label(ec, "Set what is loaded in this CFS slot", &lv_font_montserrat_12,
                             lv_color_hex(COLOR_MUTED));
  lv_obj_set_pos(edit_sub, px(24), px(46));

  const char *captions[3] = {"BRAND", "MATERIAL", "FILAMENT"};
  lv_obj_t **dds[3] = {&brand_dd, &type_dd, &name_dd};
  for (int i = 0; i < 3; ++i) {
    const int y = 76 + i * 76;
    lv_obj_t *cap = label(ec, captions[i], &lv_font_montserrat_12, lv_color_hex(COLOR_MUTED));
    lv_obj_set_pos(cap, px(24), px(y));
    lv_obj_t *dd = lv_dropdown_create(ec);
    *dds[i] = dd;
    lv_obj_set_pos(dd, px(24), px(y + 20));
    lv_obj_set_size(dd, px(304), px(42));
    style_select(dd);
    choice_handles[i] = {this, i};
    lv_obj_add_event_cb(dd, [](lv_event_t *e) {
      SlotHandle *h = static_cast<SlotHandle *>(e->user_data);
      h->panel->handle_edit_choice(h->index);
    }, LV_EVENT_VALUE_CHANGED, &choice_handles[i]);
    // Long lists (Creality PLA has 14 names) scroll inside the popup instead of leaving the screen.
    lv_obj_add_event_cb(dd, [](lv_event_t *e) {
      lv_obj_t *list = lv_dropdown_get_list(lv_event_get_target(e));
      if (list != NULL) {
        lv_obj_set_style_max_height(list, px(190), LV_PART_MAIN);
      }
    }, LV_EVENT_READY, NULL);
  }

  lv_obj_t *color_cap = label(ec, "COLOR", &lv_font_montserrat_12, lv_color_hex(COLOR_MUTED));
  lv_obj_set_pos(color_cap, px(360), px(76));
  for (int i = 0; i < 12; ++i) {
    lv_obj_t *sw = lv_obj_create(ec);
    swatches[i] = sw;
    lv_obj_remove_style_all(sw);
    lv_obj_clear_flag(sw, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(sw, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(sw, px(36), px(36));
    lv_obj_set_pos(sw, px(364 + (i % 6) * 48), px(100 + (i / 6) * 48));
    lv_obj_set_style_radius(sw, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(sw, lv_color_hex(PALETTE[i]), 0);
    lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, 0);
    lv_obj_set_style_outline_width(sw, px(2), 0);
    lv_obj_set_style_outline_pad(sw, px(2), 0);
    lv_obj_set_style_outline_color(sw, lv_color_hex(COLOR_ACCENT), 0);
    lv_obj_set_style_outline_opa(sw, LV_OPA_TRANSP, 0);
    swatch_handles[i] = {this, PALETTE[i]};
    lv_obj_add_event_cb(sw, [](lv_event_t *e) {
      ColorHandle *h = static_cast<ColorHandle *>(e->user_data);
      h->panel->pick_color(h->color);
    }, LV_EVENT_CLICKED, &swatch_handles[i]);
  }

  hex_preview = lv_obj_create(ec);
  lv_obj_remove_style_all(hex_preview);
  lv_obj_clear_flag(hex_preview, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(hex_preview, px(34), px(34));
  lv_obj_set_pos(hex_preview, px(364), px(212));
  lv_obj_set_style_radius(hex_preview, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(hex_preview, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(hex_preview, 1, 0);
  lv_obj_set_style_border_color(hex_preview, lv_color_hex(COLOR_WHITE), 0);
  lv_obj_set_style_border_opa(hex_preview, LV_OPA_30, 0);
  hex_label = label(ec, "#FFFFFF", &lv_font_montserrat_16, lv_color_hex(COLOR_FG));
  lv_obj_set_pos(hex_label, px(410), px(220));
  action_button(ec, NULL, "Custom color", ActionKind::Outline, 364, 262, 288, 44,
                [](lv_event_t *e) { static_cast<CfsPanel *>(e->user_data)->open_color_dialog(); }, this);

  action_button(ec, NULL, "Cancel", ActionKind::Outline, 24, 344, 304, 48,
                [](lv_event_t *e) { static_cast<CfsPanel *>(e->user_data)->close_edit(); }, this);
  action_button(ec, NULL, "Save", ActionKind::Primary, 364, 344, 288, 48,
                [](lv_event_t *e) { static_cast<CfsPanel *>(e->user_data)->save_edit(); }, this);

  // ---- Custom color dialog (three sliders), above the editor.
  color_root = dim_overlay(cont);
  lv_obj_t *cc = card(color_root, 146, 40, 420, 336);
  lv_obj_t *color_title = label(cc, "Custom color", &lv_font_montserrat_20, lv_color_hex(COLOR_FG));
  lv_obj_set_pos(color_title, px(24), px(16));
  color_preview = lv_obj_create(cc);
  lv_obj_remove_style_all(color_preview);
  lv_obj_clear_flag(color_preview, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(color_preview, px(48), px(48));
  lv_obj_set_pos(color_preview, px(24), px(62));
  lv_obj_set_style_radius(color_preview, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(color_preview, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(color_preview, 1, 0);
  lv_obj_set_style_border_color(color_preview, lv_color_hex(COLOR_WHITE), 0);
  lv_obj_set_style_border_opa(color_preview, LV_OPA_30, 0);
  color_hex = label(cc, "#FFFFFF", &lv_font_montserrat_20, lv_color_hex(COLOR_FG));
  lv_obj_set_pos(color_hex, px(90), px(74));
  const char *channels[3] = {"R", "G", "B"};
  for (int i = 0; i < 3; ++i) {
    const int y = 132 + i * 46;
    lv_obj_t *ch = label(cc, channels[i], &lv_font_montserrat_16, lv_color_hex(COLOR_MUTED));
    lv_obj_set_pos(ch, px(24), px(y - 4));
    lv_obj_t *sl = lv_slider_create(cc);
    color_sliders[i] = sl;
    lv_slider_set_range(sl, 0, 255);
    lv_obj_set_size(sl, px(320), px(10));
    lv_obj_set_pos(sl, px(58), px(y));
    style_slider(sl);
    lv_obj_add_event_cb(sl, [](lv_event_t *e) { static_cast<CfsPanel *>(e->user_data)->handle_color_slider(); },
                        LV_EVENT_VALUE_CHANGED, this);
  }
  action_button(cc, NULL, "Cancel", ActionKind::Outline, 24, 268, 180, 46,
                [](lv_event_t *e) { static_cast<CfsPanel *>(e->user_data)->close_color_dialog(false); }, this);
  action_button(cc, NULL, "OK", ActionKind::Primary, 216, 268, 180, 46,
                [](lv_event_t *e) { static_cast<CfsPanel *>(e->user_data)->close_color_dialog(true); }, this);

  refresh();
}

CfsPanel::~CfsPanel() {
  if (status_timer != NULL) {
    lv_timer_del(status_timer);
    status_timer = NULL;
  }
  if (verify_timer != NULL) {
    lv_timer_del(verify_timer);
    verify_timer = NULL;
  }
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

void CfsPanel::update(const cfs::State &s) {
  const bool was_connected = state.connected;
  state = s;
  if (!state.connected || !was_connected) {
    user_selected = false;
  }
  refresh();
}

void CfsPanel::foreground() {
  lv_obj_clear_flag(cont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(cont);
  powerui::overlay_open("CFS", [this]() {
    close_color_dialog(false);
    close_edit();
    lv_obj_add_flag(cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_background(cont);
  });
}

void CfsPanel::handle_slot_click(int index) {
  user_selected = true;
  select_slot(index);
  // A spool that is loaded but not described yet: go straight to the editor.
  if (state.connected && state.slots[index].kind == cfs::SlotKind::Unset) {
    open_edit(index);
  }
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

  const std::string version = "Box firmware: " + (state.version.empty() ? std::string("unknown") : state.version);
  lv_label_set_text(version_label, version.c_str());

  for (int i = 0; i < 4; ++i) {
    const cfs::Slot &s = state.slots[i];
    SlotWidget &w = slots[i];
    cfs_ui::spool_set(w.spool, s, state.active == i);
    lv_label_set_text(w.name, s.name.c_str());
    lv_obj_set_style_text_color(w.name, lv_color_hex(s.kind == cfs::SlotKind::Empty ? COLOR_MUTED : COLOR_FG), 0);
    lv_label_set_text(w.sub, slot_subtitle(s).c_str());
    if (s.kind == cfs::SlotKind::Empty) {
      lv_obj_add_flag(w.pencil, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_clear_flag(w.pencil, LV_OBJ_FLAG_HIDDEN);
    }
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
      info = "A spool is loaded. Set its material and color with Edit slot.";
      break;
    case cfs::SlotKind::Defined:
      info = sel.brand;
      if (!sel.material.empty()) {
        info += (info.empty() ? "" : ", ") + sel.material;
      }
      break;
  }
  lv_label_set_text(detail_info, info.c_str());
  lv_label_set_text(detail_remain, sel.remain_len >= 0 ? (std::to_string(sel.remain_len) + " m remaining").c_str() : "");
  if (sel.kind == cfs::SlotKind::Empty) {
    lv_obj_add_flag(detail_edit_btn, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_clear_flag(detail_edit_btn, LV_OBJ_FLAG_HIDDEN);
  }
}

// ------------------------------------------------------------------ editor --

void CfsPanel::fill_brands() {
  brand_list.clear();
  for (const auto &m : cfs::materials()) {
    if (std::find(brand_list.begin(), brand_list.end(), m.brand) == brand_list.end()) {
      brand_list.push_back(m.brand);
    }
  }
  if (brand_list.empty()) {
    return;
  }
  auto it = std::find(brand_list.begin(), brand_list.end(), edit_brand);
  const size_t sel = it == brand_list.end() ? 0 : static_cast<size_t>(it - brand_list.begin());
  edit_brand = brand_list[sel];
  std::string options;
  for (size_t i = 0; i < brand_list.size(); ++i) {
    options += (i == 0 ? "" : "\n") + brand_list[i];
  }
  lv_dropdown_set_options(brand_dd, options.c_str());
  lv_dropdown_set_selected(brand_dd, static_cast<uint16_t>(sel));
}

void CfsPanel::fill_types() {
  type_list.clear();
  for (const auto &m : cfs::materials()) {
    if (m.brand == edit_brand && std::find(type_list.begin(), type_list.end(), m.type) == type_list.end()) {
      type_list.push_back(m.type);
    }
  }
  if (type_list.empty()) {
    return;
  }
  auto it = std::find(type_list.begin(), type_list.end(), edit_type);
  const size_t sel = it == type_list.end() ? 0 : static_cast<size_t>(it - type_list.begin());
  edit_type = type_list[sel];
  std::string options;
  for (size_t i = 0; i < type_list.size(); ++i) {
    options += (i == 0 ? "" : "\n") + type_list[i];
  }
  lv_dropdown_set_options(type_dd, options.c_str());
  lv_dropdown_set_selected(type_dd, static_cast<uint16_t>(sel));
}

void CfsPanel::fill_names() {
  name_list.clear();
  for (const auto &m : cfs::materials()) {
    if (m.brand == edit_brand && m.type == edit_type) {
      name_list.push_back(m);
    }
  }
  if (name_list.empty()) {
    return;
  }
  size_t sel = 0;
  for (size_t i = 0; i < name_list.size(); ++i) {
    if (name_list[i].id == edit_material_id) {
      sel = i;
    }
  }
  edit_material_id = name_list[sel].id;
  std::string options;
  for (size_t i = 0; i < name_list.size(); ++i) {
    options += (i == 0 ? "" : "\n") + name_list[i].name;
  }
  lv_dropdown_set_options(name_dd, options.c_str());
  lv_dropdown_set_selected(name_dd, static_cast<uint16_t>(sel));
}

void CfsPanel::update_color_ui() {
  for (int i = 0; i < 12; ++i) {
    lv_obj_set_style_outline_opa(swatches[i], PALETTE[i] == (edit_color & 0xFFFFFF) ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
  }
  lv_obj_set_style_bg_color(hex_preview, lv_color_hex(edit_color), 0);
  lv_label_set_text(hex_label, hex_text(edit_color).c_str());
}

void CfsPanel::open_edit(int index) {
  if (index < 0 || index > 3 || !state.connected) {
    return;
  }
  if (printing_now()) {
    show_status("Cannot edit slots while printing", COLOR_WARNING, 5000);
    return;
  }
  if (cfs::materials().empty()) {
    show_status("Material database not found on this printer", COLOR_WARNING, 6000);
    return;
  }
  select_slot(index);
  edit_slot = index;
  const cfs::Slot &s = state.slots[index];

  // Start from what the slot holds; a slot with nothing known starts as Generic PLA, white.
  cfs::Material current;
  if (!s.material_id.empty() && cfs::find_material(s.material_id, current)) {
    edit_brand = current.brand;
    edit_type = current.type;
    edit_material_id = current.id;
  } else {
    edit_brand = "Generic";
    edit_type = "PLA";
    edit_material_id.clear();
  }
  edit_color = s.has_color ? s.color : PALETTE[0];
  color_touched = s.has_color;

  fill_brands();
  fill_types();
  fill_names();
  if (!color_touched) {
    for (const auto &m : name_list) {
      if (m.id == edit_material_id && m.has_color) {
        edit_color = m.color;
      }
    }
  }
  update_color_ui();
  lv_label_set_text(edit_title, ("Edit slot " + slot_letter(index)).c_str());
  lv_obj_clear_flag(edit_root, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(edit_root);
}

void CfsPanel::close_edit() {
  if (edit_root != NULL) {
    lv_obj_add_flag(edit_root, LV_OBJ_FLAG_HIDDEN);
  }
  edit_slot = -1;
}

void CfsPanel::handle_edit_choice(int which) {
  if (which == 0) {
    const uint16_t i = lv_dropdown_get_selected(brand_dd);
    if (i < brand_list.size()) {
      edit_brand = brand_list[i];
      fill_types();
      fill_names();
    }
  } else if (which == 1) {
    const uint16_t i = lv_dropdown_get_selected(type_dd);
    if (i < type_list.size()) {
      edit_type = type_list[i];
      fill_names();
    }
  } else {
    const uint16_t i = lv_dropdown_get_selected(name_dd);
    if (i < name_list.size()) {
      edit_material_id = name_list[i].id;
    }
  }
  if (!color_touched) {
    for (const auto &m : name_list) {
      if (m.id == edit_material_id && m.has_color) {
        edit_color = m.color;
      }
    }
    update_color_ui();
  }
}

void CfsPanel::pick_color(uint32_t color) {
  edit_color = color & 0xFFFFFF;
  color_touched = true;
  update_color_ui();
}

void CfsPanel::open_color_dialog() {
  color_draft = edit_color & 0xFFFFFF;
  lv_slider_set_value(color_sliders[0], (color_draft >> 16) & 0xFF, LV_ANIM_OFF);
  lv_slider_set_value(color_sliders[1], (color_draft >> 8) & 0xFF, LV_ANIM_OFF);
  lv_slider_set_value(color_sliders[2], color_draft & 0xFF, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(color_preview, lv_color_hex(color_draft), 0);
  lv_label_set_text(color_hex, hex_text(color_draft).c_str());
  lv_obj_clear_flag(color_root, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(color_root);
}

void CfsPanel::handle_color_slider() {
  color_draft = (static_cast<uint32_t>(lv_slider_get_value(color_sliders[0])) << 16)
                | (static_cast<uint32_t>(lv_slider_get_value(color_sliders[1])) << 8)
                | static_cast<uint32_t>(lv_slider_get_value(color_sliders[2]));
  lv_obj_set_style_bg_color(color_preview, lv_color_hex(color_draft), 0);
  lv_label_set_text(color_hex, hex_text(color_draft).c_str());
}

void CfsPanel::close_color_dialog(bool apply) {
  if (color_root == NULL) {
    return;
  }
  lv_obj_add_flag(color_root, LV_OBJ_FLAG_HIDDEN);
  if (apply) {
    pick_color(color_draft);
  }
}

void CfsPanel::save_edit() {
  if (edit_slot < 0 || !state.connected || edit_material_id.empty()) {
    return;
  }
  if (printing_now()) {
    show_status("Cannot edit slots while printing", COLOR_WARNING, 5000);
    close_edit();
    return;
  }
  const std::string num(1, static_cast<char>('A' + edit_slot));
  const std::string color = hex_text(edit_color);
  // The same two commands the Creality interface sends: the other fields of the slot follow from the material id.
  ws.gcode_script(fmt::format("BOX_MODIFY_TN_DATA ADDR={} NUM={} PART=material_type DATA={}", state.box, num,
                              edit_material_id));
  ws.gcode_script(fmt::format("BOX_MODIFY_TN_DATA ADDR={} NUM={} PART=color_value DATA={}", state.box, num, color));
  spdlog::info("cfs: slot {} set to material {} color {}", num, edit_material_id, color);

  pending.slot = edit_slot;
  pending.material_id = edit_material_id;
  pending.color = edit_color & 0xFFFFFF;
  close_edit();
  show_status("Saving slot " + num + "...", COLOR_MUTED, 0);
  if (verify_timer != NULL) {
    lv_timer_del(verify_timer);
  }
  verify_timer = lv_timer_create(&CfsPanel::verify_timer_cb, 2500, this);
  lv_timer_set_repeat_count(verify_timer, 1);
}

void CfsPanel::check_saved() {
  if (pending.slot < 0 || pending.slot > 3) {
    return;
  }
  const cfs::Slot &s = state.slots[pending.slot];
  const std::string num = slot_letter(pending.slot);
  const bool ok = s.material_id == pending.material_id && s.has_color && s.color == pending.color;
  if (ok) {
    show_status("Slot " + num + " saved", COLOR_ACCENT, 6000);
  } else {
    show_status("Slot " + num + ": the CFS has not reported the change", COLOR_WARNING, 8000);
    spdlog::warn("cfs: slot {} did not report material {} color {} (has id '{}')", num, pending.material_id,
                 hex_text(pending.color), s.material_id);
  }
  pending.slot = -1;
}

void CfsPanel::show_status(const std::string &text, uint32_t color, uint32_t clear_after_ms) {
  if (status_label == NULL) {
    return;
  }
  lv_label_set_text(status_label, text.c_str());
  lv_obj_set_style_text_color(status_label, lv_color_hex(color), 0);
  if (status_timer != NULL) {
    lv_timer_del(status_timer);
    status_timer = NULL;
  }
  if (clear_after_ms > 0) {
    status_timer = lv_timer_create(&CfsPanel::status_timer_cb, clear_after_ms, this);
    lv_timer_set_repeat_count(status_timer, 1);
  }
}

void CfsPanel::status_timer_cb(lv_timer_t *timer) {
  CfsPanel *panel = static_cast<CfsPanel *>(timer->user_data);
  panel->status_timer = NULL;
  if (panel->status_label != NULL) {
    lv_label_set_text(panel->status_label, "");
  }
}

void CfsPanel::verify_timer_cb(lv_timer_t *timer) {
  CfsPanel *panel = static_cast<CfsPanel *>(timer->user_data);
  panel->verify_timer = NULL;
  panel->check_saved();
}
