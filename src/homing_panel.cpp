#include "homing_panel.h"
#include "state.h"
#include "spdlog/spdlog.h"
#include "config.h"

#include <ctime>

static const float distances[] = {0.1, 0.5, 1, 5, 10, 25, 50};

namespace {
constexpr uint32_t HOMING_PANEL_BACKGROUND = 0x282B30;
constexpr uint32_t BUTTON_GREY = 0x555555;
constexpr uint32_t BUTTON_GREY_PRESSED = 0x3A3A3A;
}

LV_IMG_DECLARE(arrow_left);
LV_IMG_DECLARE(arrow_up);
LV_IMG_DECLARE(arrow_right);
LV_IMG_DECLARE(arrow_down);
LV_IMG_DECLARE(home);
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
  const auto width_scale = (double)lv_disp_get_physical_hor_res(NULL) / 800.0;
  const auto height_scale = (double)lv_disp_get_physical_ver_res(NULL) / 480.0;
  const lv_coord_t square_width = static_cast<lv_coord_t>(130 * width_scale);
  const lv_coord_t square_height = static_cast<lv_coord_t>(130 * height_scale);
  const lv_coord_t horizontal_gap = static_cast<lv_coord_t>(30 * width_scale);
  const lv_coord_t vertical_gap = static_cast<lv_coord_t>(25 * height_scale);
  const lv_coord_t selector_gap = static_cast<lv_coord_t>(25 * height_scale);
  const lv_coord_t safety_gap = static_cast<lv_coord_t>(14 * width_scale);
  const lv_coord_t safety_top = static_cast<lv_coord_t>(48 * height_scale);
  const lv_coord_t right_margin = static_cast<lv_coord_t>(14 * width_scale);
  const lv_coord_t title_height = static_cast<lv_coord_t>(32 * height_scale);
  const lv_coord_t motion_width = square_width * 4 + horizontal_gap * 3;
  const lv_coord_t safety_width = square_width;
  const lv_coord_t emergency_height = static_cast<lv_coord_t>(160 * height_scale);
  const lv_coord_t back_height = static_cast<lv_coord_t>(100 * height_scale);
  const lv_coord_t safety_height = emergency_height + square_height + back_height + safety_gap * 2;
  const lv_coord_t selector_y = square_height * 2 + vertical_gap + selector_gap;
  const lv_coord_t motion_height = selector_y + static_cast<lv_coord_t>(96 * height_scale);
  const lv_coord_t available_height = static_cast<lv_coord_t>(lv_disp_get_physical_ver_res(NULL)) - title_height;
  const lv_coord_t motion_top = title_height + (available_height - motion_height) / 2;

  lv_obj_clear_flag(homing_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(homing_cont, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_pad_all(homing_cont, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_color(homing_cont, lv_color_hex(HOMING_PANEL_BACKGROUND), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(homing_cont, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(homing_cont, 0, LV_PART_MAIN);

  lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(title_bar, LV_PCT(100), 32);
  lv_obj_set_pos(title_bar, 0, 0);
  lv_obj_set_style_pad_all(title_bar, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_color(title_bar, lv_color_hex(BUTTON_GREY), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(title_bar, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(title_bar, 0, LV_PART_MAIN);

  lv_label_set_text(title_label, "HOMING CONTROL");
  lv_obj_set_width(title_label, LV_PCT(100));
  lv_label_set_long_mode(title_label, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_align(title_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_style_text_color(title_label, lv_color_white(), LV_PART_MAIN);
  lv_obj_set_style_text_font(title_label, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_align(title_label, LV_ALIGN_CENTER, 0, 0);

  lv_obj_set_width(time_label, LV_SIZE_CONTENT);
  lv_obj_set_style_text_color(time_label, lv_color_white(), LV_PART_MAIN);
  lv_obj_set_style_text_font(time_label, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_align(time_label, LV_ALIGN_RIGHT_MID, -10, 0);
  update_clock();
  clock_timer = lv_timer_create(&HomingPanel::_update_clock_cb, 1000, this);

  auto style_group = [horizontal_gap](lv_obj_t *group) {
    lv_obj_clear_flag(group, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(group, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_column(group, horizontal_gap, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(group, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(group, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(group, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(group, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  };

  auto style_vertical_group = [safety_gap](lv_obj_t *group) {
    lv_obj_clear_flag(group, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(group, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(group, safety_gap, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(group, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(group, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(group, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(group, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  };

  auto style_button = [](ButtonContainer &button) {
    lv_obj_t *container = button.get_container();
    lv_obj_set_style_bg_color(container, lv_color_hex(BUTTON_GREY), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(container, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(container, lv_color_hex(BUTTON_GREY_PRESSED), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(container, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_radius(container, 12, LV_PART_MAIN);
    lv_obj_set_style_border_width(container, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(button.get_button(), LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(button.get_button(), LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_PRESSED);
  };

  lv_obj_clear_flag(motion_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(motion_cont, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(motion_cont, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(motion_cont, 0, LV_PART_MAIN);
  style_group(motion_top_cont);
  style_group(motion_bottom_cont);
  style_vertical_group(safety_cont);
  lv_obj_set_size(motion_top_cont, motion_width, square_height);
  lv_obj_set_size(motion_bottom_cont, motion_width, square_height);
  lv_obj_set_size(motion_cont, motion_width, motion_height);
  lv_obj_set_size(safety_cont, safety_width, safety_height);

  for (ButtonContainer *button : {&home_all_btn, &home_xy_btn, &x_up_btn, &x_down_btn,
                                  &y_up_btn, &y_down_btn, &z_up_btn, &z_down_btn,
                                  &motoroff_btn}) {
    button->set_fixed_size(square_width, square_height);
    style_button(*button);
  }
  emergency_btn.set_fixed_size(safety_width, emergency_height);
  back_btn.set_fixed_size(safety_width, back_height);
  style_button(emergency_btn);
  style_button(back_btn);

  lv_obj_set_style_bg_color(home_all_btn.get_container(), lv_color_hex(0x4CAF50),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(home_all_btn.get_container(), lv_color_hex(0x388E3C),
                            LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_color(home_xy_btn.get_container(), lv_color_hex(0x4CAF50),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(home_xy_btn.get_container(), lv_color_hex(0x388E3C),
                            LV_PART_MAIN | LV_STATE_PRESSED);

  lv_obj_set_style_border_width(emergency_btn.get_container(), 2, LV_PART_MAIN);
  lv_obj_set_style_border_color(emergency_btn.get_container(), lv_color_hex(0xF44336), LV_PART_MAIN);

  lv_obj_align(motion_cont, LV_ALIGN_TOP_RIGHT,
               -(safety_width + safety_gap + right_margin), motion_top);
  lv_obj_align(motion_top_cont, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_align(motion_bottom_cont, LV_ALIGN_TOP_MID, 0, square_height + vertical_gap);
  lv_obj_set_width(distance_selector.get_container(), motion_width);
  lv_obj_align(distance_selector.get_container(), LV_ALIGN_TOP_MID, 0, selector_y);
  lv_obj_align(safety_cont, LV_ALIGN_TOP_RIGHT, -right_margin, safety_top);

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
