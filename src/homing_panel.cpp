#include "homing_panel.h"
#include "powerui.h"
#include "state.h"
#include "spdlog/spdlog.h"
#include "config.h"

#include <ctime>

static const float distances[] = {0.1, 0.5, 1, 5, 10, 25, 50};

LV_IMG_DECLARE(arrow_left);
LV_IMG_DECLARE(arrow_up);
LV_IMG_DECLARE(arrow_right);
LV_IMG_DECLARE(arrow_down);
LV_IMG_DECLARE(home);
LV_IMG_DECLARE(home_z);
LV_IMG_DECLARE(back);
LV_IMG_DECLARE(z_closer);
LV_IMG_DECLARE(z_farther);
LV_IMG_DECLARE(emergency);
LV_IMG_DECLARE(motor_off_img);

HomingPanel::HomingPanel(KWebSocketClient &websocket_client, std::mutex &lock)
  : NotifyConsumer(lock)
  , ws(websocket_client)
  , homing_cont(lv_obj_create(lv_scr_act()))
  , title_bar(lv_obj_create(homing_cont))
  , title_label(lv_label_create(title_bar))
  , time_label(lv_label_create(title_bar))
  , clock_timer(NULL)
  , motion_cont(lv_obj_create(homing_cont))
  , motion_top_cont(lv_obj_create(motion_cont))
  , motion_bottom_cont(lv_obj_create(motion_cont))
  , safety_cont(lv_obj_create(homing_cont))
  , home_all_btn(motion_top_cont, &home, "Home All", &HomingPanel::_handle_callback, this)
  , y_up_btn(motion_top_cont, &arrow_up, "Y+", &HomingPanel::_handle_callback, this)
  , home_xy_btn(motion_top_cont, &home, "Home XY", &HomingPanel::_handle_callback, this)
  , z_down_btn(motion_top_cont, &z_farther, "Z-", &HomingPanel::_handle_callback, this)
  , home_z_btn(motion_top_cont, &home_z, "Home Z", &HomingPanel::_handle_callback, this)
  , x_down_btn(motion_bottom_cont, &arrow_left, "X-", &HomingPanel::_handle_callback, this)
  , y_down_btn(motion_bottom_cont, &arrow_down, "Y-", &HomingPanel::_handle_callback, this)
  , x_up_btn(motion_bottom_cont, &arrow_right, "X+", &HomingPanel::_handle_callback, this)
  , z_up_btn(motion_bottom_cont, &z_closer, "Z+", &HomingPanel::_handle_callback, this)
  , emergency_btn(safety_cont, &emergency, "Emergency\nStop", &HomingPanel::_handle_callback, this,
		  "Do you want to emergency stop?",
		  [&websocket_client]() {
		    spdlog::debug("emergency stop pressed");
		    websocket_client.send_jsonrpc("printer.emergency_stop");
		  })
  , motoroff_btn(safety_cont, &motor_off_img, "Motor Off", &HomingPanel::_handle_callback, this)
  , back_btn(safety_cont, &back, "Back", &HomingPanel::_handle_callback, this)
  , distance_selector(motion_cont, "Move Distance (mm)",
		     {".1", ".5", "1", "5", "10", "25", "50", ""}, 2, 70, 15, &HomingPanel::_handle_selector_cb, this)
{
  // PowerUI layout for the overlay area (736 x 440): cross pad, Z column, actions column and a distance bar.
  using namespace powerui;
  lv_obj_clear_flag(homing_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(homing_cont, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_pad_all(homing_cont, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(homing_cont, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_color(homing_cont, lv_color_hex(COLOR_BG), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(homing_cont, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(homing_cont, 0, LV_PART_MAIN);

  // The main title bar replaces the panel's own one; the clock timer keeps running on the hidden label.
  lv_obj_add_flag(title_bar, LV_OBJ_FLAG_HIDDEN);
  update_clock();
  clock_timer = lv_timer_create(&HomingPanel::_update_clock_cb, 1000, this);

  lv_obj_add_flag(motion_cont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(safety_cont, LV_OBJ_FLAG_HIDDEN);

  lv_obj_t *xy_card = card(homing_cont, 12, 12, 332, 344);
  lv_obj_t *z_card = card(homing_cont, 356, 12, 128, 344);
  lv_obj_t *actions_card = card(homing_cont, 496, 12, 228, 344);
  lv_obj_t *distance_card = card(homing_cont, 12, 368, 712, 60);

  auto place = [](ButtonContainer &button, lv_obj_t *parent, int x, int y, int w, int h) {
    lv_obj_set_parent(button.get_container(), parent);
    button.set_fixed_size(px(w), px(h));
    lv_obj_set_pos(button.get_container(), px(x), px(y));
  };

  // XY cross: Y+ on top, X- / Home XY / X+ in the middle, Y- below (cells of 96 x 100 with 8 px gaps).
  const int cx = 13, cy = 13, cw = 96, ch = 100, cg = 8;
  place(y_up_btn, xy_card, cx + (cw + cg), cy, cw, ch);
  place(x_down_btn, xy_card, cx, cy + (ch + cg), cw, ch);
  place(home_xy_btn, xy_card, cx + (cw + cg), cy + (ch + cg), cw, ch);
  place(x_up_btn, xy_card, cx + 2 * (cw + cg), cy + (ch + cg), cw, ch);
  place(y_down_btn, xy_card, cx + (cw + cg), cy + 2 * (ch + cg), cw, ch);

  // Z column.
  place(z_up_btn, z_card, 12, 12, 102, 100);
  place(home_z_btn, z_card, 12, 120, 102, 100);
  place(z_down_btn, z_card, 12, 228, 102, 100);

  // Actions: Home All, Motor Off, Emergency Stop and the Back pill.
  place(home_all_btn, actions_card, 12, 12, 202, 100);
  place(motoroff_btn, actions_card, 12, 120, 202, 100);
  place(emergency_btn, actions_card, 12, 228, 202, 100);

  for (ButtonContainer *button : {&y_up_btn, &x_down_btn, &x_up_btn, &y_down_btn, &z_up_btn, &z_down_btn, &motoroff_btn}) {
    style_button(button->get_container(), button->get_button(), ButtonKind::Outline);
  }
  style_button(home_xy_btn.get_container(), home_xy_btn.get_button(), ButtonKind::Soft);
  style_button(home_all_btn.get_container(), home_all_btn.get_button(), ButtonKind::Soft);
  style_button(home_z_btn.get_container(), home_z_btn.get_button(), ButtonKind::Soft);
  style_button(emergency_btn.get_container(), emergency_btn.get_button(), ButtonKind::Destructive);

  // Move distance bar: label on the left, segmented selector on the right.
  lv_obj_t *distance_label = label(distance_card, "Distance (mm)", &lv_font_montserrat_14, lv_color_hex(COLOR_MUTED));
  lv_obj_align(distance_label, LV_ALIGN_LEFT_MID, px(14), 0);

  lv_obj_t *selector_cont = distance_selector.get_container();
  lv_obj_set_parent(selector_cont, distance_card);
  lv_obj_add_flag(distance_selector.get_label(), LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_size(selector_cont, px(572), px(44));
  lv_obj_align(selector_cont, LV_ALIGN_RIGHT_MID, -px(12), 0);
  lv_obj_set_size(distance_selector.get_selector(), px(572), px(44));
  style_segmented(distance_selector.get_selector());

  ws.register_notify_update(this);
}

HomingPanel::~HomingPanel() {
  if (clock_timer != NULL) {
    lv_timer_del(clock_timer);
    clock_timer = NULL;
  }

  if (homing_cont != NULL) {
    lv_obj_del(homing_cont);
    homing_cont = NULL;
  }

  ws.unregister_notify_update(this);
}

void HomingPanel::consume(json &j) {
  auto v = j["/params/0/toolhead/homed_axes"_json_pointer];
  if (!v.is_null()) {
    std::string homed_axes = v.template get<std::string>();
    std::lock_guard<std::mutex> lock(lv_lock);
    if (homed_axes.find("x") != std::string::npos) {
      x_up_btn.enable();
      x_down_btn.enable();
    } else {
      x_up_btn.disable();
      x_down_btn.disable();
    }

    if (homed_axes.find("y") != std::string::npos) {
      y_up_btn.enable();
      y_down_btn.enable();
    } else {
      y_up_btn.disable();
      y_down_btn.disable();
    }

    if (homed_axes.find("z") != std::string::npos) {
      z_up_btn.enable();
      z_down_btn.enable();
    } else {
      z_up_btn.disable();
      z_down_btn.disable();
    }
  }
}

lv_obj_t *HomingPanel::get_container() {
  return homing_cont;
}

void HomingPanel::foreground() {
  auto v = State::get_instance()
    ->get_data("/printer_state/toolhead/homed_axes"_json_pointer);
  if (!v.is_null()) {
    std::string homed_axes = v.template get<std::string>();
    if (homed_axes.find("x") != std::string::npos) {
      x_up_btn.enable();
      x_down_btn.enable();
    } else {
      x_up_btn.disable();
      x_down_btn.disable();
    }

    if (homed_axes.find("y") != std::string::npos) {
      y_up_btn.enable();
      y_down_btn.enable();
    } else {
      y_up_btn.disable();
      y_down_btn.disable();
    }

    //Set the Z axis buttons
    z_up_btn.set_image(&z_farther);
    z_down_btn.set_image(&z_closer);

    if (homed_axes.find("z") != std::string::npos) {
      z_up_btn.enable();
      z_down_btn.enable();
    } else {
      z_up_btn.disable();
      z_down_btn.disable();
    }
  }

  //Set the Z axis buttons

  v = Config::get_instance()->get_json("/invert_z_icon");
  bool inverted = !v.is_null() && v.template get<bool>();
  if (inverted) {
    // UP arrow
    z_up_btn.set_image(&z_farther);
    z_down_btn.set_image(&z_closer);
  } else {
    // DOWN arrow
    z_up_btn.set_image(&z_closer);
    z_down_btn.set_image(&z_farther);
  }

  lv_obj_move_foreground(homing_cont);
  lv_obj_add_flag(back_btn.get_container(), LV_OBJ_FLAG_HIDDEN);  // Back lives in the title bar
  powerui::overlay_open("Movement", [this]() { lv_obj_move_background(homing_cont); });
}

void HomingPanel::update_clock() {
  const std::time_t now = std::time(nullptr);
  const std::tm local_time = *std::localtime(&now);
  char time_text[6] = {};
  std::strftime(time_text, sizeof(time_text), "%H:%M", &local_time);
  lv_label_set_text(time_label, time_text);
}

void HomingPanel::handle_callback(lv_event_t *event) {
  lv_obj_t *btn = lv_event_get_current_target(event);  
  const char * distance = lv_btnmatrix_get_btn_text(distance_selector.get_selector(),
						    distance_selector.get_selected_idx());
  std::string move_op;

  if (btn == home_all_btn.get_container()) {
    spdlog::debug("home all pressed");
    ws.gcode_script("G28 X Y Z");

  }
  else if (btn == home_xy_btn.get_container()) {
    spdlog::debug("home xy pressed");
    ws.gcode_script("G28 X Y");

  }
  else if (btn == y_up_btn.get_container()) {
    spdlog::debug("y up pressed");
    move_op = fmt::format("G0 Y+{} F1200", distance);

  }
  else if (btn == y_down_btn.get_container()) {
    spdlog::debug("y down pressed");
    move_op = fmt::format("G0 Y-{} F1200", distance);

  }
  else if (btn == x_up_btn.get_container()) {
    spdlog::debug("x up pressed");
    move_op = fmt::format("G0 X+{} F1200", distance);

  }
  else if (btn == x_down_btn.get_container()) {
    spdlog::debug("x down pressed");
    move_op = fmt::format("G0 X-{} F1200", distance);

  }
  else if (btn == z_up_btn.get_container()) {
    spdlog::debug("z up pressed");
    move_op = fmt::format("G0 Z+{} F1200", distance);

  }
  else if (btn == z_down_btn.get_container()) {
    spdlog::debug("z down pressed");
    move_op = fmt::format("G0 Z-{} F1200", distance);

  }
  else if (btn == home_z_btn.get_container()) {
    spdlog::debug("home z pressed");
    ws.gcode_script("G28 Z");

  }
  else if (btn == emergency_btn.get_container()) {
    spdlog::debug("emergency stop pressed");
    ws.send_jsonrpc("printer.emergency_stop");

  }
  else if (btn == motoroff_btn.get_container()) {
    spdlog::debug("motor off pressed");
    ws.gcode_script("M84");

  } else if (btn == back_btn.get_container()) {
    lv_obj_move_background(homing_cont);
  }
  else {
    spdlog::debug("Unknown action button pressed");
  }

  if (move_op.size() > 0) {
    // ws.gcode_script("G91");
    ws.gcode_script(fmt::format("G91\n{}", move_op));
  }
}

void HomingPanel::handle_selector_cb(lv_event_t *event) {
  lv_obj_t * obj = lv_event_get_target(event);
  uint32_t idx = lv_btnmatrix_get_selected_btn(obj);
  distance_selector.set_selected_idx(idx);
  spdlog::debug("selector move distance index {}", idx);
}
