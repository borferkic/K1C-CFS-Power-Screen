#include "pid_tune_panel.h"
#include "powerui.h"
#include "state.h"
#include "utils.h"
#include "spdlog/spdlog.h"

#include <cmath>

LV_IMG_DECLARE(extruder);
LV_IMG_DECLARE(bed);
LV_IMG_DECLARE(ui_icon_sliders);
LV_IMG_DECLARE(back);

using namespace powerui;

PidTunePanel::PidTunePanel(KWebSocketClient &c, std::mutex &l)
  : NotifyConsumer(l)
  , ws(c)
  , cont(lv_obj_create(lv_scr_act()))
  , notice_title(NULL)
  , notice_text(NULL)
  , back_btn(cont, &back, "Back", &PidTunePanel::_handle_callback, this)
  , printing(false)
  , tuning(false)
{
  lv_obj_move_background(cont);
  style_overlay_root(cont);
  lv_obj_add_flag(back_btn.get_container(), LV_OBJ_FLAG_HIDDEN);  // Back lives in the title bar

  hotend.heater = "extruder";
  bed_card.heater = "heater_bed";
  build_card(hotend, 12, "Hotend", &extruder, lv_color_hex(COLOR_EXTRUDER), {200, 220, 240, 260}, 220);
  build_card(bed_card, 374, "Bed", &::bed, lv_color_hex(COLOR_BED), {50, 60, 70, 80}, 60);

  // Notice card (orange border).
  lv_obj_t *notice = card(cont, 12, 354, 712, 74);
  lv_obj_set_style_border_color(notice, lv_color_hex(COLOR_WARNING), 0);
  lv_obj_set_style_border_opa(notice, LV_OPA_40, 0);
  notice_title = label(notice, "The printer heats to the selected temperature", &lv_font_montserrat_14, lv_color_hex(COLOR_FG));
  lv_obj_set_pos(notice_title, px(20), px(14));
  notice_text = label(notice, "Not available while printing. The result is saved with SAVE_CONFIG and restarts Klipper.",
                      &lv_font_montserrat_12, lv_color_hex(COLOR_MUTED));
  lv_obj_set_pos(notice_text, px(20), px(40));

  ws.register_notify_update(this);
}

PidTunePanel::~PidTunePanel() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
  ws.unregister_notify_update(this);
}

void PidTunePanel::build_card(HeaterCard &c, int x, const char *title, const lv_img_dsc_t *icon_src, lv_color_t icon_color,
                              const std::vector<int> &options, int default_option) {
  c.options = options;
  for (int o : options) {
    c.texts.push_back(std::to_string(o));
  }
  for (const auto &t : c.texts) {
    c.map.push_back(t.c_str());
  }
  c.map.push_back("");

  lv_obj_t *k = card(cont, x, 12, 350, 328);

  lv_obj_t *tile = plain(k);
  lv_obj_set_size(tile, px(44), px(44));
  lv_obj_set_pos(tile, px(20), px(18));
  lv_obj_set_style_radius(tile, px(10), 0);
  lv_obj_set_style_bg_color(tile, lv_color_hex(COLOR_SECONDARY), 0);
  lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
  lv_obj_center(icon(tile, icon_src, 28, icon_color));

  lv_obj_t *name = label(k, title, &lv_font_montserrat_20, lv_color_hex(COLOR_FG));
  lv_obj_set_pos(name, px(76), px(18));
  lv_obj_t *sub = label(k, "Heater PID", &lv_font_montserrat_12, lv_color_hex(COLOR_MUTED));
  lv_obj_set_pos(sub, px(76), px(44));

  c.current = label(k, "--", &lv_font_montserrat_24, lv_color_hex(COLOR_FG));
  lv_obj_align(c.current, LV_ALIGN_TOP_RIGHT, -px(20), px(16));
  lv_obj_t *current_tag = label(k, "current", &lv_font_montserrat_12, lv_color_hex(COLOR_MUTED));
  lv_obj_align(current_tag, LV_ALIGN_TOP_RIGHT, -px(20), px(46));

  lv_obj_t *target_title = label(k, "Target temperature (\xC2\xB0""C)", &lv_font_montserrat_14, lv_color_hex(COLOR_MUTED));
  lv_obj_set_pos(target_title, px(20), px(98));

  c.btnm = lv_btnmatrix_create(k);
  lv_btnmatrix_set_map(c.btnm, c.map.data());
  lv_btnmatrix_set_btn_ctrl_all(c.btnm, LV_BTNMATRIX_CTRL_CHECKABLE);
  lv_btnmatrix_set_one_checked(c.btnm, true);
  lv_obj_set_size(c.btnm, px(310), px(48));
  lv_obj_set_pos(c.btnm, px(20), px(124));
  style_segmented(c.btnm);
  for (size_t i = 0; i < c.options.size(); i++) {
    if (c.options[i] == default_option) {
      lv_btnmatrix_set_btn_ctrl(c.btnm, i, LV_BTNMATRIX_CTRL_CHECKED);
    }
  }

  c.start = action_button(k, &ui_icon_sliders, "Start PID tune", ActionKind::Primary, 20, 256, 310, 52,
                          &PidTunePanel::_handle_callback, this);
  c.start_label = lv_obj_get_child(c.start, 1);
}

void PidTunePanel::init(json &j) {
  (void)j;
}

int PidTunePanel::selected_target(HeaterCard &c) {
  const uint32_t idx = lv_btnmatrix_get_selected_btn(c.btnm);
  if (idx < c.options.size() && lv_btnmatrix_has_btn_ctrl(c.btnm, idx, LV_BTNMATRIX_CTRL_CHECKED)) {
    return c.options[idx];
  }
  for (size_t i = 0; i < c.options.size(); i++) {
    if (lv_btnmatrix_has_btn_ctrl(c.btnm, i, LV_BTNMATRIX_CTRL_CHECKED)) {
      return c.options[i];
    }
  }
  return c.options.front();
}

void PidTunePanel::set_printing(bool p) {
  printing = p;
  lv_label_set_text(notice_title, p ? "Not available while printing" : "The printer heats to the selected temperature");
  for (HeaterCard *c : {&hotend, &bed_card}) {
    if (p || tuning) {
      lv_obj_add_state(c->start, LV_STATE_DISABLED);
    } else {
      lv_obj_clear_state(c->start, LV_STATE_DISABLED);
    }
  }
}

void PidTunePanel::set_tuning(bool t) {
  tuning = t;
  for (HeaterCard *c : {&hotend, &bed_card}) {
    lv_label_set_text(c->start_label, t ? "Tuning..." : "Start PID tune");
    if (t || printing) {
      lv_obj_add_state(c->start, LV_STATE_DISABLED);
    } else {
      lv_obj_clear_state(c->start, LV_STATE_DISABLED);
    }
  }
}

void PidTunePanel::foreground() {
  State *s = State::get_instance();
  auto st = s->get_data("/printer_state/print_stats/state"_json_pointer);
  const std::string state = st.is_string() ? st.template get<std::string>() : "";
  set_printing(state == "printing" || state == "paused");
  set_tuning(false);

  for (HeaterCard *c : {&hotend, &bed_card}) {
    auto t = s->get_data(json::json_pointer(fmt::format("/printer_state/{}/temperature", c->heater)));
    if (t.is_number()) {
      lv_label_set_text(c->current, fmt::format("{:.0f}\xC2\xB0", t.template get<double>()).c_str());
    }
  }

  lv_obj_move_foreground(cont);
  powerui::overlay_open("PID Tune", [this]() { lv_obj_move_background(cont); });
}

void PidTunePanel::consume(json &j) {
  std::lock_guard<std::mutex> lock(lv_lock);
  auto st = j["/params/0/print_stats/state"_json_pointer];
  if (st.is_string()) {
    const std::string state = st.template get<std::string>();
    set_printing(state == "printing" || state == "paused");
  }
  for (HeaterCard *c : {&hotend, &bed_card}) {
    auto t = j[json::json_pointer(fmt::format("/params/0/{}/temperature", c->heater))];
    if (t.is_number()) {
      lv_label_set_text(c->current, fmt::format("{:.0f}\xC2\xB0", t.template get<double>()).c_str());
    }
  }
}

void PidTunePanel::start_tune(HeaterCard &c) {
  const int target = selected_target(c);
  const bool is_hotend = &c == &hotend;
  const std::string what = is_hotend ? "hotend" : "bed";
  const std::string message = fmt::format(
    "The {} heats to {} \xC2\xB0""C and the PID tune runs for several minutes. When it finishes the result is saved "
    "and Klipper restarts.", what, target);

  confirm_dialog("Start PID tune?", message.c_str(), "Start", ActionKind::Primary, [this, is_hotend, target]() {
    std::string script;
    if (is_hotend) {
      if (!KUtils::is_homed()) {
        script += "G28\n";
      }
      script += fmt::format("G1 Z10 F600\nM106 S255\nPID_CALIBRATE HEATER=extruder TARGET={}\nM107\nSAVE_CONFIG", target);
    } else {
      script = fmt::format("PID_CALIBRATE HEATER=heater_bed TARGET={}\nSAVE_CONFIG", target);
    }
    spdlog::debug("pid tune: {}", script);
    set_tuning(true);
    json params = {{"script", script}};
    ws.send_jsonrpc("printer.gcode.script", params, [this](json &) {
      std::lock_guard<std::mutex> lock(lv_lock);
      set_tuning(false);
    });
  });
}

void PidTunePanel::handle_callback(lv_event_t *event) {
  lv_obj_t *btn = lv_event_get_current_target(event);
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
    return;
  }
  if (btn == hotend.start) {
    start_tune(hotend);
  } else if (btn == bed_card.start) {
    start_tune(bed_card);
  }
}
