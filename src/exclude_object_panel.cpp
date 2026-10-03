#include "exclude_object_panel.h"
#include "powerui.h"
#include "state.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cmath>

LV_IMG_DECLARE(ui_icon_exclude);

namespace {
// Map of the bed (right card), in design pixels.
constexpr int MAP_W = 268;
constexpr int MAP_H = 268;
constexpr int MAP_PAD = 10;
}  // namespace

ExcludeObjectPanel::ExcludeObjectPanel(KWebSocketClient &websocket_client, std::mutex &l)
  : NotifyConsumer(l)
  , ws(websocket_client)
  , cont(lv_obj_create(lv_scr_act()))
  , list(NULL)
  , map(NULL)
  , empty(NULL)
  , confirm(NULL)
  , confirm_title(NULL)
  , confirm_cancel(NULL)
  , confirm_ok(NULL)
  , sync_timer(NULL)
{
  using namespace powerui;

  lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
  lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(cont, 0, 0);
  lv_obj_set_style_radius(cont, 0, 0);
  lv_obj_set_style_border_width(cont, 0, 0);
  lv_obj_set_style_bg_color(cont, lv_color_hex(COLOR_BG), 0);
  lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, 0);
  lv_obj_add_flag(cont, LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_CLICKABLE);

  // ---- Objects card (left).
  lv_obj_t *list_card = card(cont, 12, 12, 400, 416);
  lv_obj_t *list_title = label(list_card, "Objects", &lv_font_montserrat_16, lv_color_hex(COLOR_FG));
  lv_obj_set_pos(list_title, px(20), px(18));
  list = lv_obj_create(list_card);
  lv_obj_remove_style_all(list);
  lv_obj_set_size(list, px(376), px(348));
  lv_obj_set_pos(list, px(12), px(54));
  lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(list, LV_DIR_VER);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(list, px(8), 0);
  empty = empty_state(list_card, &ui_icon_exclude, "No objects",
                      "This file has no labeled objects. Enable \"Label objects\" in the slicer.");
  lv_obj_add_flag(empty, LV_OBJ_FLAG_HIDDEN);

  // ---- Bed map card (right).
  lv_obj_t *map_card = card(cont, 424, 12, 300, 416);
  lv_obj_t *map_title = label(map_card, "Bed map", &lv_font_montserrat_16, lv_color_hex(COLOR_FG));
  lv_obj_set_pos(map_title, px(20), px(18));
  map = lv_obj_create(map_card);
  lv_obj_remove_style_all(map);
  lv_obj_clear_flag(map, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(map, px(MAP_W), px(MAP_H));
  lv_obj_set_pos(map, px(16), px(54));
  lv_obj_set_style_bg_color(map, lv_color_hex(COLOR_BG), 0);
  lv_obj_set_style_bg_opa(map, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(map, px(12), 0);
  lv_obj_set_style_border_width(map, 1, 0);
  lv_obj_set_style_border_color(map, lv_color_hex(COLOR_WHITE), 0);
  lv_obj_set_style_border_opa(map, LV_OPA_10, 0);
  lv_obj_t *note = label(map_card, "Excluding an object cannot be undone until the next print.", &lv_font_montserrat_12,
                         lv_color_hex(COLOR_MUTED));
  lv_label_set_long_mode(note, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(note, px(260));
  lv_obj_set_pos(note, px(20), px(336));

  // ---- Confirmation (shown over the whole panel).
  confirm = lv_obj_create(cont);
  lv_obj_remove_style_all(confirm);
  lv_obj_set_size(confirm, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(confirm, lv_color_hex(COLOR_BLACK), 0);
  lv_obj_set_style_bg_opa(confirm, LV_OPA_70, 0);
  lv_obj_add_flag(confirm, LV_OBJ_FLAG_CLICKABLE);  // swallows touches while asking
  lv_obj_clear_flag(confirm, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t *dlg = card(confirm, 0, 0, 380, 190);
  lv_obj_center(dlg);
  lv_obj_set_style_border_opa(dlg, LV_OPA_30, 0);
  confirm_title = label(dlg, "Exclude object?", &lv_font_montserrat_20, lv_color_hex(COLOR_FG));
  lv_obj_set_pos(confirm_title, px(24), px(22));
  lv_obj_t *msg = label(dlg, "It will not be printed. This cannot be undone until the next print.", &lv_font_montserrat_14,
                        lv_color_hex(COLOR_MUTED));
  lv_label_set_long_mode(msg, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(msg, px(332));
  lv_obj_set_pos(msg, px(24), px(62));
  auto make_button = [this, dlg](const char *text, bool danger, int x) {
    lv_obj_t *b = lv_btn_create(dlg);
    lv_obj_set_size(b, px(156), px(44));
    lv_obj_set_pos(b, px(x), px(122));
    lv_obj_set_style_radius(b, px(10), LV_PART_MAIN);
    lv_obj_set_style_shadow_width(b, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(b, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(b, lv_color_hex(danger ? powerui::COLOR_DANGER : powerui::COLOR_SECONDARY), LV_PART_MAIN);
    lv_obj_set_style_bg_color(b, lv_color_hex(danger ? powerui::COLOR_MAT_RED_DARK : powerui::COLOR_PRESSED), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_t *t = powerui::label(b, text, &lv_font_montserrat_16, lv_color_hex(powerui::COLOR_WHITE));
    lv_obj_center(t);
    lv_obj_add_event_cb(b, &ExcludeObjectPanel::_handle_event, LV_EVENT_CLICKED, this);
    return b;
  };
  confirm_cancel = make_button("Cancel", false, 24);
  confirm_ok = make_button("Exclude", true, 200);
  lv_obj_add_flag(confirm, LV_OBJ_FLAG_HIDDEN);

  lv_obj_move_background(cont);

  ws.register_notify_update(this);
  sync_timer = lv_timer_create(&ExcludeObjectPanel::_sync_cb, 2000, this);
}

ExcludeObjectPanel::~ExcludeObjectPanel() {
  ws.unregister_notify_update(this);
  if (sync_timer != NULL) {
    lv_timer_del(sync_timer);
    sync_timer = NULL;
  }
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

void ExcludeObjectPanel::set_availability_callback(std::function<void(bool)> callback) {
  availability_cb = callback;
}

void ExcludeObjectPanel::foreground() {
  sync_from_state();
  rebuild();
  lv_obj_add_flag(confirm, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(cont);
  powerui::overlay_open("Exclude Object", [this]() {
    lv_obj_add_flag(confirm, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_background(cont);
  });
}

std::string ExcludeObjectPanel::display_name(const std::string &name) {
  std::string out = name;
  std::replace(out.begin(), out.end(), '_', ' ');
  return out.empty() ? "Object" : out;
}

// Reads the objects, the excluded ones and the current one. Klipper reports only what changed, so each field is optional.
void ExcludeObjectPanel::merge(const json &status) {
  if (!status.is_object()) {
    return;
  }
  if (status.contains("objects") && status["objects"].is_array()) {
    std::vector<Object> next;
    for (const auto &o : status["objects"]) {
      Object ob;
      ob.name = o.value("name", std::string());
      bool have_box = false;
      if (o.contains("polygon") && o["polygon"].is_array()) {
        for (const auto &pt : o["polygon"]) {
          if (!pt.is_array() || pt.size() < 2 || !pt[0].is_number() || !pt[1].is_number()) {
            continue;
          }
          const double x = pt[0].get<double>(), y = pt[1].get<double>();
          if (!have_box) {
            ob.x0 = ob.x1 = x;
            ob.y0 = ob.y1 = y;
            have_box = true;
          } else {
            ob.x0 = std::min(ob.x0, x);
            ob.x1 = std::max(ob.x1, x);
            ob.y0 = std::min(ob.y0, y);
            ob.y1 = std::max(ob.y1, y);
          }
        }
      }
      if (!have_box && o.contains("center") && o["center"].is_array() && o["center"].size() >= 2) {
        const double cx = o["center"][0].get<double>(), cy = o["center"][1].get<double>();
        ob.x0 = cx - 4;
        ob.x1 = cx + 4;
        ob.y0 = cy - 4;
        ob.y1 = cy + 4;
      }
      for (const auto &old : objects) {
        if (old.name == ob.name) {
          ob.excluded = old.excluded;
        }
      }
      next.push_back(ob);
    }
    objects = next;
  }
  if (status.contains("excluded_objects") && status["excluded_objects"].is_array()) {
    for (auto &ob : objects) {
      ob.excluded = false;
    }
    for (const auto &n : status["excluded_objects"]) {
      if (!n.is_string()) {
        continue;
      }
      for (auto &ob : objects) {
        if (ob.name == n.get<std::string>()) {
          ob.excluded = true;
        }
      }
    }
  }
  if (status.contains("current_object")) {
    current_object = status["current_object"].is_string() ? status["current_object"].get<std::string>() : std::string();
  }
}

void ExcludeObjectPanel::load_bed_size() {
  auto &lo = State::get_instance()->get_data("/printer_state/toolhead/axis_minimum"_json_pointer);
  auto &hi = State::get_instance()->get_data("/printer_state/toolhead/axis_maximum"_json_pointer);
  if (lo.is_array() && hi.is_array() && lo.size() >= 2 && hi.size() >= 2 && lo[0].is_number() && lo[1].is_number() &&
      hi[0].is_number() && hi[1].is_number() && hi[0].get<double>() > lo[0].get<double>() &&
      hi[1].get<double>() > lo[1].get<double>()) {
    bed_min_x = lo[0].get<double>();
    bed_min_y = lo[1].get<double>();
    bed_max_x = hi[0].get<double>();
    bed_max_y = hi[1].get<double>();
  }
}

std::string ExcludeObjectPanel::signature() const {
  std::string sig = current_object + "|";
  for (const auto &o : objects) {
    sig += o.name + (o.excluded ? "!" : "") + ",";
  }
  return sig;
}

void ExcludeObjectPanel::update_availability() {
  if (objects.size() != last_count) {
    last_count = objects.size();
    if (count_cb) {
      count_cb(last_count);
    }
  }
  const bool available = !objects.empty();
  if (available != last_available) {
    last_available = available;
    if (availability_cb) {
      availability_cb(available);
    }
  }
}

void ExcludeObjectPanel::sync_from_state() {
  auto &full = State::get_instance()->get_data("/printer_state/exclude_object"_json_pointer);
  if (full.is_object()) {
    merge(full);
  }
  update_availability();
  const std::string sig = signature();
  if (sig != last_signature) {
    last_signature = sig;
    rebuild();
  }
}

void ExcludeObjectPanel::consume(json &j) {
  std::lock_guard<std::mutex> lock(lv_lock);
  auto status = j["/params/0/exclude_object"_json_pointer];
  if (status.is_null()) {
    return;
  }
  merge(status);
  update_availability();
  const std::string sig = signature();
  if (sig != last_signature) {
    last_signature = sig;
    rebuild();
  }
}

void ExcludeObjectPanel::rebuild() {
  if (list == NULL || map == NULL) {
    return;
  }
  using namespace powerui;
  load_bed_size();
  lv_obj_clean(list);
  lv_obj_clean(map);
  exclude_buttons.clear();

  if (objects.empty()) {
    lv_obj_clear_flag(empty, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  lv_obj_add_flag(empty, LV_OBJ_FLAG_HIDDEN);

  // ---- Rows.
  for (size_t i = 0; i < objects.size(); ++i) {
    const Object &ob = objects[i];
    lv_obj_t *row = lv_obj_create(list);
    lv_obj_remove_style_all(row);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(row, px(376), px(64));
    lv_obj_set_style_radius(row, px(12), 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_border_opa(row, LV_OPA_10, 0);
    lv_obj_set_style_opa(row, ob.excluded ? LV_OPA_60 : LV_OPA_COVER, 0);

    lv_obj_t *index = plain(row);
    lv_obj_set_size(index, px(32), px(32));
    lv_obj_set_pos(index, px(12), px(15));
    lv_obj_set_style_radius(index, px(8), 0);
    lv_obj_set_style_bg_color(index, lv_color_hex(COLOR_SECONDARY), 0);
    lv_obj_set_style_bg_opa(index, LV_OPA_COVER, 0);
    lv_obj_t *number = label(index, std::to_string(i + 1).c_str(), &lv_font_montserrat_14, lv_color_hex(COLOR_FG));
    lv_obj_center(number);

    lv_obj_t *name = label(row, display_name(ob.name).c_str(), &lv_font_montserrat_16, lv_color_hex(COLOR_FG));
    lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
    lv_obj_set_size(name, px(200), px(22));
    lv_obj_set_pos(name, px(56), px(10));
    lv_obj_t *status = label(row, ob.excluded ? "Excluded" : (ob.name == current_object ? "Printing now" : "Printing"),
                             &lv_font_montserrat_12, lv_color_hex(COLOR_MUTED));
    lv_obj_set_pos(status, px(56), px(36));

    lv_obj_t *btn = lv_btn_create(row);
    lv_obj_set_size(btn, px(92), px(36));
    lv_obj_align(btn, LV_ALIGN_RIGHT_MID, -px(12), 0);
    lv_obj_set_style_radius(btn, px(10), LV_PART_MAIN);
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn, lv_color_hex(COLOR_DESTRUCTIVE), LV_PART_MAIN);
    lv_obj_set_style_border_opa(btn, LV_OPA_50, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, lv_color_hex(COLOR_DESTRUCTIVE_BG), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_t *btn_label = label(btn, ob.excluded ? "Excluded" : "Exclude", &lv_font_montserrat_14,
                                lv_color_hex(ob.excluded ? COLOR_MUTED : COLOR_DESTRUCTIVE));
    lv_obj_center(btn_label);
    if (ob.excluded) {
      lv_obj_add_state(btn, LV_STATE_DISABLED);
      lv_obj_set_style_border_color(btn, lv_color_hex(COLOR_WHITE), LV_PART_MAIN);
      lv_obj_set_style_border_opa(btn, LV_OPA_10, LV_PART_MAIN);
    }
    lv_obj_add_event_cb(btn, &ExcludeObjectPanel::_handle_event, LV_EVENT_CLICKED, this);
    exclude_buttons.push_back(btn);
  }

  // ---- Bed map: the bounding box of each object, scaled to the bed (Y points up on the bed, down on the screen).
  const double bed_w = std::max(1.0, bed_max_x - bed_min_x);
  const double bed_h = std::max(1.0, bed_max_y - bed_min_y);
  const double area_w = px(MAP_W - 2 * MAP_PAD), area_h = px(MAP_H - 2 * MAP_PAD);
  const double scale = std::min(area_w / bed_w, area_h / bed_h);
  const double off_x = px(MAP_PAD) + (area_w - bed_w * scale) / 2;
  const double off_y = px(MAP_PAD) + (area_h - bed_h * scale) / 2;
  const lv_coord_t min_side = px(22);
  for (size_t i = 0; i < objects.size(); ++i) {
    const Object &ob = objects[i];
    lv_coord_t w = std::max<lv_coord_t>(min_side, (lv_coord_t)((ob.x1 - ob.x0) * scale));
    lv_coord_t h = std::max<lv_coord_t>(min_side, (lv_coord_t)((ob.y1 - ob.y0) * scale));
    lv_coord_t x = (lv_coord_t)(off_x + (ob.x0 - bed_min_x) * scale);
    lv_coord_t y = (lv_coord_t)(off_y + (bed_max_y - ob.y1) * scale);
    lv_obj_t *r = lv_obj_create(map);
    lv_obj_remove_style_all(r);
    lv_obj_clear_flag(r, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(r, w, h);
    lv_obj_set_pos(r, x, y);
    lv_obj_set_style_radius(r, px(8), 0);
    lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(r, ob.name == current_object && !ob.excluded ? 2 : 1, 0);
    uint32_t tone;
    if (ob.excluded) {
      lv_obj_set_style_bg_color(r, lv_color_hex(COLOR_DESTRUCTIVE_BG), 0);
      lv_obj_set_style_border_color(r, lv_color_hex(COLOR_DESTRUCTIVE), 0);
      lv_obj_set_style_border_opa(r, LV_OPA_70, 0);
      tone = COLOR_DESTRUCTIVE;
    } else {
      lv_obj_set_style_bg_color(r, lv_color_hex(COLOR_ACCENT_BG), 0);
      lv_obj_set_style_border_color(r, lv_color_hex(COLOR_ACCENT), 0);
      lv_obj_set_style_border_opa(r, ob.name == current_object ? LV_OPA_COVER : LV_OPA_60, 0);
      tone = COLOR_ACCENT;
    }
    lv_obj_t *n = label(r, std::to_string(i + 1).c_str(), &lv_font_montserrat_14, lv_color_hex(tone));
    lv_obj_center(n);
  }
}

void ExcludeObjectPanel::ask(int index) {
  if (index < 0 || index >= (int)objects.size() || objects[index].excluded) {
    return;
  }
  pending = index;
  lv_label_set_text(confirm_title, ("Exclude " + display_name(objects[index].name) + "?").c_str());
  lv_obj_clear_flag(confirm, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(confirm);
}

void ExcludeObjectPanel::do_exclude() {
  if (pending < 0 || pending >= (int)objects.size()) {
    return;
  }
  Object &ob = objects[pending];
  spdlog::debug("excluding object {}", ob.name);
  ws.gcode_script("EXCLUDE_OBJECT NAME=" + ob.name);
  ob.excluded = true;  // shown right away; Klipper confirms with the next status update
  pending = -1;
  last_signature = signature();
  rebuild();
}

void ExcludeObjectPanel::handle_event(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
    return;
  }
  lv_obj_t *target = lv_event_get_current_target(event);
  if (target == confirm_cancel) {
    pending = -1;
    lv_obj_add_flag(confirm, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  if (target == confirm_ok) {
    lv_obj_add_flag(confirm, LV_OBJ_FLAG_HIDDEN);
    do_exclude();
    return;
  }
  for (size_t i = 0; i < exclude_buttons.size(); ++i) {
    if (target == exclude_buttons[i]) {
      ask((int)i);
      return;
    }
  }
}
