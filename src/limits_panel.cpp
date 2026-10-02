#include "limits_panel.h"
#include "powerui.h"
#include "state.h"
#include "spdlog/spdlog.h"

LV_IMG_DECLARE(ui_icon_reset);
LV_IMG_DECLARE(back);

LimitsPanel::LimitsPanel(KWebSocketClient &c, std::mutex &l)
  : NotifyConsumer(l)
  , ws(c)
  , cont(lv_obj_create(lv_scr_act()))
  , limit_cont(lv_obj_create(cont))
  , back_btn(cont, &back, "Back", &LimitsPanel::_handle_callback, this)
  , max_velocity_default(1000)
  , max_accel_default(20000)
  , max_accel_to_decel_default(10000)
  , square_corner_default(5)
{
  lv_obj_move_background(cont);
  powerui::style_overlay_root(cont);

  lv_obj_set_pos(limit_cont, 0, 0);
  lv_obj_set_size(limit_cont, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_pad_all(limit_cont, 0, 0);
  lv_obj_set_style_border_width(limit_cont, 0, 0);
  lv_obj_set_style_bg_opa(limit_cont, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(limit_cont, LV_OBJ_FLAG_SCROLLABLE);

  velocity = create_card(12, 12, "Velocity", "mm/s", max_velocity_default);
  acceleration = create_card(374, 12, "Acceleration", "mm/s2", max_accel_default);
  square_corner = create_card(12, 226, "Square corner velocity", "mm/s", square_corner_default);
  accel_to_decel = create_card(374, 226, "Acceleration to deceleration", "mm/s2", max_accel_to_decel_default);

  lv_obj_add_flag(back_btn.get_container(), LV_OBJ_FLAG_HIDDEN);  // Back lives in the title bar
  
  ws.register_notify_update(this);
}

LimitsPanel::~LimitsPanel() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

void LimitsPanel::LimitCard::set_range(int min_range, int max_range) {
  lv_slider_set_range(slider, min_range, max_range);
  lv_label_set_text(min_label, std::to_string(min_range).c_str());
  lv_label_set_text(max_label, std::to_string(max_range).c_str());
}

void LimitsPanel::LimitCard::set_default(int v) {
  lv_label_set_text(default_label, fmt::format("Default: {} {}", v, unit).c_str());
}

void LimitsPanel::LimitCard::update_value(int v) {
  lv_label_set_text(value, std::to_string(v).c_str());
  lv_slider_set_value(slider, v, LV_ANIM_ON);
}

void LimitsPanel::_handle_value_changed(lv_event_t *event) {
  lv_obj_t *slider = lv_event_get_target(event);
  lv_obj_t *value = (lv_obj_t*)event->user_data;
  lv_label_set_text(value, std::to_string((int)lv_slider_get_value(slider)).c_str());
}

LimitsPanel::LimitCard LimitsPanel::create_card(int x, int y, const char *name, const char *unit, int max_range) {
  using namespace powerui;
  LimitCard c;
  c.unit = unit;
  c.card = card(limit_cont, x, y, 350, 202);

  lv_obj_t *title = label(c.card, name, &lv_font_montserrat_14, lv_color_hex(COLOR_MUTED));
  lv_obj_set_pos(title, px(20), px(18));

  c.reset = action_button(c.card, &ui_icon_reset, "Reset", ActionKind::Outline, 246, 12, 90, 32, &LimitsPanel::_handle_callback, this);

  // Value and unit share a flex row so the unit stays attached to the end of the number.
  lv_obj_t *value_row = plain(c.card);
  lv_obj_set_pos(value_row, px(20), px(58));
  lv_obj_set_size(value_row, px(310), px(52));
  lv_obj_set_flex_flow(value_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(value_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
  lv_obj_set_style_pad_column(value_row, px(8), 0);
  c.value = label(value_row, "0", &lv_font_montserrat_40, lv_color_hex(COLOR_FG));
  lv_obj_t *unit_label = label(value_row, unit, &lv_font_montserrat_14, lv_color_hex(COLOR_MUTED));
  lv_obj_set_style_pad_bottom(unit_label, px(6), 0);

  c.slider = lv_slider_create(c.card);
  lv_obj_set_size(c.slider, px(310), px(14));
  lv_obj_set_pos(c.slider, px(20), px(122));
  lv_slider_set_range(c.slider, 0, max_range);
  style_slider(c.slider);
  lv_obj_add_event_cb(c.slider, &LimitsPanel::_handle_value_changed, LV_EVENT_VALUE_CHANGED, c.value);
  lv_obj_add_event_cb(c.slider, &LimitsPanel::_handle_callback, LV_EVENT_RELEASED, this);

  c.min_label = label(c.card, "0", &lv_font_montserrat_12, lv_color_hex(COLOR_MUTED));
  lv_obj_set_pos(c.min_label, px(20), px(150));
  c.max_label = label(c.card, std::to_string(max_range).c_str(), &lv_font_montserrat_12, lv_color_hex(COLOR_MUTED));
  lv_obj_align(c.max_label, LV_ALIGN_TOP_RIGHT, -px(20), px(150));
  c.default_label = label(c.card, "", &lv_font_montserrat_12, lv_color_hex(COLOR_MUTED));
  lv_obj_align(c.default_label, LV_ALIGN_BOTTOM_LEFT, px(20), -px(14));
  c.set_default(max_range);
  return c;
}

void LimitsPanel::init(json &j) {
  State *s = State::get_instance();
  auto v = s->get_data("/printer_state/configfile/settings/printer"_json_pointer);
  if (!v.is_null()) {
    if (v.contains("max_velocity")) {
      max_velocity_default = v["max_velocity"].template get<int>();      
      velocity.set_default(max_velocity_default);
      velocity.set_range(1, max_velocity_default);
    }

    if (v.contains("max_accel")) {
      max_accel_default = v["max_accel"].template get<int>();
      acceleration.set_default(max_accel_default);
      acceleration.set_range(1, max_accel_default);
    }

    if (v.contains("max_accel_to_decel")) {
      max_accel_to_decel_default = v["max_accel_to_decel"].template get<int>();
      accel_to_decel.set_default(max_accel_to_decel_default);
      accel_to_decel.set_range(1, max_accel_to_decel_default);
    }

    if (v.contains("square_corner_velocity")) {
      square_corner_default = v["square_corner_velocity"].template get<int>();
      square_corner.set_default(square_corner_default);
      square_corner.set_range(0, square_corner_default);
    }
    
  }
  
  v = j["/result/status/toolhead/max_velocity"_json_pointer];
  if (!v.is_null()) {
    velocity.update_value(v.template get<int>());
  }

  v = j["/result/status/toolhead/max_accel"_json_pointer];
  if (!v.is_null()) {
    acceleration.update_value(v.template get<int>());
  }

  v = j["/result/status/toolhead/max_accel_to_decel"_json_pointer];
  if (!v.is_null()) {
    accel_to_decel.update_value(v.template get<int>());
  }

  v = j["/result/status/toolhead/square_corner_velocity"_json_pointer];
  if (!v.is_null()) {
    square_corner.update_value(v.template get<int>());
  }
}

void LimitsPanel::foreground() {
  lv_obj_move_foreground(cont);
  lv_obj_add_flag(back_btn.get_container(), LV_OBJ_FLAG_HIDDEN);  // Back lives in the title bar
  powerui::overlay_open("Limits", [this]() { lv_obj_move_background(cont); });
}

void LimitsPanel::consume(json &j) {
  std::lock_guard<std::mutex> lock(lv_lock);
  auto v = j["/params/0/toolhead/max_velocity"_json_pointer];
  if (!v.is_null()) {
    velocity.update_value(v.template get<int>());
  }

  v = j["/params/0/toolhead/max_accel"_json_pointer];
  if (!v.is_null()) {
    acceleration.update_value(v.template get<int>());
  }

  v = j["/params/0/toolhead/max_accel_to_decel"_json_pointer];
  if (!v.is_null()) {
    accel_to_decel.update_value(v.template get<int>());
  }

  v = j["/params/0/toolhead/square_corner_velocity"_json_pointer];
  if (!v.is_null()) {
    square_corner.update_value(v.template get<int>());
  }
  
}

void LimitsPanel::handle_callback(lv_event_t *e) {
  lv_obj_t *btn = lv_event_get_current_target(e);

  if (btn == back_btn.get_container()) {
    lv_obj_move_background(cont);
    return;
  }

  if (lv_event_get_code(e) == LV_EVENT_RELEASED) {
    lv_obj_t *obj = lv_event_get_target(e);
    int v = lv_slider_get_value(obj);

    if (obj == velocity.get_slider()) {
      ws.gcode_script(fmt::format("SET_VELOCITY_LIMIT VELOCITY={}", v));

    } else if (obj == acceleration.get_slider()) {
      ws.gcode_script(fmt::format("SET_VELOCITY_LIMIT ACCEL={}", v));

    } else if (obj == accel_to_decel.get_slider()) {
      ws.gcode_script(fmt::format("SET_VELOCITY_LIMIT ACCEL_TO_DECEL={}", v));
      
    } else if (obj == square_corner.get_slider()) {
      ws.gcode_script(fmt::format("SET_VELOCITY_LIMIT SQUARE_CORNER_VELOCITY={}", v));

    }
    
  } else if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    if (btn == velocity.get_off()) {
      ws.gcode_script(fmt::format("SET_VELOCITY_LIMIT VELOCITY={}", max_velocity_default));

    } else if (btn == acceleration.get_off()) {
      ws.gcode_script(fmt::format("SET_VELOCITY_LIMIT ACCEL={}", max_accel_default));

    } else if (btn == accel_to_decel.get_off()) {
      ws.gcode_script(fmt::format("SET_VELOCITY_LIMIT ACCEL_TO_DECEL={}", max_accel_to_decel_default));
      
    } else if (btn == square_corner.get_off()) {
      ws.gcode_script(fmt::format("SET_VELOCITY_LIMIT SQUARE_CORNER_VELOCITY={}", square_corner_default));

    }
  }
}
