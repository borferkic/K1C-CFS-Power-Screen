#include "extruder_panel.h"
#include "state.h"
#include "config.h"
#include "spdlog/spdlog.h"

#include <limits>

LV_IMG_DECLARE(back);
LV_IMG_DECLARE(spoolman_img);
LV_IMG_DECLARE(extrude_img);
LV_IMG_DECLARE(retract_img);
LV_IMG_DECLARE(unload_filament_img);
LV_IMG_DECLARE(load_filament_img);
LV_IMG_DECLARE(filament_img);
LV_IMG_DECLARE(extruder);
LV_IMG_DECLARE(cooldown_img);

ExtruderPanel::ExtruderPanel(KWebSocketClient &websocket_client,
			     std::mutex &lock,
			     Numpad &numpad,
			     SpoolmanPanel &sm)
  : NotifyConsumer(lock)
  , ws(websocket_client)
  , panel_cont(lv_obj_create(lv_scr_act()))
  , title_bar(lv_obj_create(panel_cont))
  , title_label(lv_label_create(title_bar))
  , time_label(lv_label_create(title_bar))
  , clock_timer(NULL)
  , spoolman_panel(sm)
  , extruder_temp(ws, panel_cont, &extruder, 150,
	  "EXTRUDER", lv_palette_main(LV_PALETTE_RED), false, true, numpad, "extruder", NULL, NULL)
  , temp_selector(panel_cont, "EXTRUDER TEMPERATURE (C)",
		  {"180", "190", "200", "210", "220", "230", "240", ""}, 6, &ExtruderPanel::_handle_callback, this)
  , length_selector(panel_cont, "EXTRUDE LENGTH (MM)",
		    {"5", "10", "15", "20", "25", "30", "35", ""}, 1, &ExtruderPanel::_handle_callback, this)
  , speed_selector(panel_cont, "EXTRUDE SPEED (MM/S)",
		   {"1", "2", "5", "10", "25", "35", "50", ""}, 2, &ExtruderPanel::_handle_callback, this)
  , rightside_btns_cont(lv_obj_create(panel_cont))
  , leftside_btns_cont(lv_obj_create(panel_cont))
  , load_btn(leftside_btns_cont, &load_filament_img, "LOAD", &ExtruderPanel::_handle_callback, this)
  , unload_btn(leftside_btns_cont, &unload_filament_img, "UNLOAD", &ExtruderPanel::_handle_callback, this)
  , cooldown_btn(leftside_btns_cont, &cooldown_img, "COOL", &ExtruderPanel::_handle_callback, this)
  , manual_change_btn(leftside_btns_cont, NULL, "MANUAL\nCOLOR", &ExtruderPanel::_handle_callback, this)
  , spoolman_btn(rightside_btns_cont, NULL, "CFS", &ExtruderPanel::_handle_callback, this)
  , extrude_btn(rightside_btns_cont, &extrude_img, "EXTRUDE", &ExtruderPanel::_handle_callback, this)
  , retract_btn(rightside_btns_cont, &retract_img, "RETRACT", &ExtruderPanel::_handle_callback, this)
  , back_btn(rightside_btns_cont, &back, "BACK", &ExtruderPanel::_handle_callback, this)
  , load_filament_macro("LOAD_FILAMENT")
  , unload_filament_macro("UNLOAD_FILAMENT")
  , cooldown_macro("SET_HEATER_TEMPERATURE HEATER=extruder TARGET=0")
{
  Config *conf = Config::get_instance();
  auto df = conf->get_json("/default_printer");
  if (!df.empty()) {
    auto v = conf->get_json(conf->df() + "default_macros/load_filament");
    if (!v.is_null()) {
      load_filament_macro = v.template get<std::string>();
    }

    v = conf->get_json(conf->df() + "default_macros/unload_filament");
    if (!v.is_null()) {
      unload_filament_macro = v.template get<std::string>();
    }

    v = conf->get_json(conf->df() + "default_macros/cooldown");
    if (!v.is_null()) {
      cooldown_macro = v.template get<std::string>();
    }
  }

  lv_obj_move_background(panel_cont);
  lv_obj_clear_flag(panel_cont, LV_OBJ_FLAG_SCROLLABLE);  
  lv_obj_set_size(panel_cont, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_pad_all(panel_cont, 0, 0);
  lv_obj_set_style_pad_row(panel_cont, 1, 0);

  lv_obj_add_flag(title_bar, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(title_bar, LV_PCT(100), 32);
  lv_obj_set_pos(title_bar, 0, 0);
  lv_obj_set_style_pad_all(title_bar, 0, 0);
  lv_obj_set_style_bg_color(title_bar, lv_color_hex(0x555555), 0);
  lv_obj_set_style_bg_opa(title_bar, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(title_bar, 0, 0);

  lv_label_set_text(title_label, "FILAMENT CONTROL");
  lv_obj_set_width(title_label, LV_PCT(100));
  lv_label_set_long_mode(title_label, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_align(title_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_color(title_label, lv_color_white(), 0);
  lv_obj_set_style_text_font(title_label, &lv_font_montserrat_20, 0);
  lv_obj_align(title_label, LV_ALIGN_CENTER, 0, 0);

  lv_obj_set_width(time_label, LV_SIZE_CONTENT);
  lv_obj_set_style_text_color(time_label, lv_color_white(), 0);
  lv_obj_set_style_text_font(time_label, &lv_font_montserrat_20, 0);
  lv_obj_align(time_label, LV_ALIGN_RIGHT_MID, -10, 0);
  update_clock();
  clock_timer = lv_timer_create(&ExtruderPanel::_update_clock_cb, 1000, this);

  auto width_scale = (double)lv_disp_get_physical_hor_res(NULL) / 800.0;
  auto height_scale = (double)lv_disp_get_physical_ver_res(NULL) / 480.0;
  load_btn.set_fixed_size(130 * width_scale, 100);
  unload_btn.set_fixed_size(130 * width_scale, 100);
  cooldown_btn.set_fixed_size(130 * width_scale, 100);
  extrude_btn.set_fixed_size(130 * width_scale, 100);
  retract_btn.set_fixed_size(130 * width_scale, 100);
  back_btn.set_fixed_size(130 * width_scale, 100);
  lv_obj_set_width(extruder_temp.get_sensor(), 130 * width_scale);
  lv_obj_set_height(extruder_temp.get_sensor(), 32 * height_scale);
  extruder_temp.set_current_only(75 * width_scale, 5 * width_scale);
  extruder_temp.set_image_zoom(100);

  lv_obj_set_size(rightside_btns_cont, LV_PCT(20), LV_SIZE_CONTENT);
  lv_obj_set_style_pad_row(rightside_btns_cont, 15, 0);
  lv_obj_set_flex_flow(rightside_btns_cont, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(rightside_btns_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(rightside_btns_cont, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_set_size(leftside_btns_cont, LV_PCT(20), LV_SIZE_CONTENT);
  lv_obj_set_style_pad_row(leftside_btns_cont, 15, 0);
  lv_obj_set_flex_flow(leftside_btns_cont, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(leftside_btns_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(leftside_btns_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_move_to_index(manual_change_btn.get_container(), 0);
  
  spoolman_btn.disable();  

  auto set_button_background = [](lv_obj_t *container, lv_obj_t *button,
                                  lv_color_t normal, lv_color_t pressed) {
    lv_obj_set_style_bg_color(container, normal, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(container, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(container, pressed, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(container, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_radius(container, 12, LV_PART_MAIN);
    lv_obj_set_style_border_width(container, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(button, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(button, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(button, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DISABLED);
  };

  const auto button_grey = lv_color_hex(0x555555);
  const auto button_grey_pressed = lv_color_hex(0x3A3A3A);
  set_button_background(load_btn.get_container(), load_btn.get_button(), button_grey, button_grey_pressed);
  set_button_background(unload_btn.get_container(), unload_btn.get_button(), button_grey, button_grey_pressed);
  set_button_background(extrude_btn.get_container(), extrude_btn.get_button(), button_grey, button_grey_pressed);
  set_button_background(retract_btn.get_container(), retract_btn.get_button(), button_grey, button_grey_pressed);
  set_button_background(back_btn.get_container(), back_btn.get_button(), button_grey, button_grey_pressed);
  set_button_background(cooldown_btn.get_container(), cooldown_btn.get_button(),
                        lv_color_hex(0x4FC3F7), lv_color_hex(0x0288D1));
  set_button_background(manual_change_btn.get_container(), manual_change_btn.get_button(),
                        lv_color_hex(0x4CAF50), lv_color_hex(0x388E3C));
  set_button_background(spoolman_btn.get_container(), spoolman_btn.get_button(),
                        lv_color_hex(0x4CAF50), lv_color_hex(0x388E3C));
  lv_obj_set_style_bg_color(spoolman_btn.get_container(), button_grey, LV_PART_MAIN | LV_STATE_DISABLED);
  lv_obj_set_style_bg_opa(spoolman_btn.get_container(), LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DISABLED);

  lv_obj_set_width(manual_change_btn.get_container(), 130 * width_scale);
  lv_obj_set_size(manual_change_btn.get_button(), 130 * width_scale, 60);
  lv_obj_set_width(spoolman_btn.get_container(), 130 * width_scale);
  lv_obj_set_size(spoolman_btn.get_button(), 130 * width_scale, 60);

  static lv_coord_t grid_main_row_dsc[] = {32, LV_GRID_FR(6), LV_GRID_FR(6), LV_GRID_FR(6),
    LV_GRID_TEMPLATE_LAST};
  static lv_coord_t grid_main_col_dsc[] = {LV_GRID_FR(2), LV_GRID_FR(7), LV_GRID_FR(2), LV_GRID_TEMPLATE_LAST};
  
  lv_obj_clear_flag(panel_cont, LV_OBJ_FLAG_SCROLLABLE);
  
  lv_obj_set_grid_dsc_array(panel_cont, grid_main_col_dsc, grid_main_row_dsc);
  lv_obj_add_flag(extruder_temp.get_sensor(), LV_OBJ_FLAG_FLOATING);
  lv_obj_align(extruder_temp.get_sensor(), LV_ALIGN_TOP_LEFT, 0, 0);

  // lv_obj_set_size(extruder_temp.get_sensor(), 350, 60);
  // col 0
  // lv_obj_set_grid_cell(spoolman_btn.get_container(), LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_START, 0, 2);
  // lv_obj_set_grid_cell(load_btn.get_container(), LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_END, 0, 2);
  // lv_obj_set_grid_cell(unload_btn.get_container(), LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_START, 2, 2);
  // lv_obj_set_grid_cell(cooldown_btn.get_container(), LV_GRID_ALIGN_END, 0, 1, LV_GRID_ALIGN_END, 2, 2);

  lv_obj_set_grid_cell(leftside_btns_cont, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 1, 3);
  
  // col 1
  // lv_obj_set_grid_cell(extruder_temp.get_sensor(), LV_GRID_ALIGN_CENTER, 0, 2, LV_GRID_ALIGN_CENTER, 0, 1);
  lv_obj_set_grid_cell(speed_selector.get_container(), LV_GRID_ALIGN_CENTER, 1, 1, LV_GRID_ALIGN_END, 1, 1);
  lv_obj_set_grid_cell(length_selector.get_container(), LV_GRID_ALIGN_CENTER, 1, 1, LV_GRID_ALIGN_CENTER, 2, 1);
  lv_obj_set_grid_cell(temp_selector.get_container(), LV_GRID_ALIGN_CENTER, 1, 1, LV_GRID_ALIGN_START, 3, 1);
  
  // col 2
  // lv_obj_set_grid_cell(spoolman_btn.get_container(), LV_GRID_ALIGN_CENTER, 2, 1, LV_GRID_ALIGN_START, 0, 2);
  // lv_obj_set_grid_cell(retract_btn.get_container(), LV_GRID_ALIGN_CENTER, 2, 1, LV_GRID_ALIGN_END, 0, 2);
  // lv_obj_set_grid_cell(extrude_btn.get_container(), LV_GRID_ALIGN_CENTER, 2, 1, LV_GRID_ALIGN_START, 2, 2);
  // lv_obj_set_grid_cell(back_btn.get_container(), LV_GRID_ALIGN_END, 2, 1, LV_GRID_ALIGN_END, 2, 2);

  lv_obj_set_grid_cell(rightside_btns_cont, LV_GRID_ALIGN_CENTER, 2, 1, LV_GRID_ALIGN_CENTER, 1, 3);
  // lv_obj_set_grid_cell(retract_btn.get_container(), LV_GRID_ALIGN_CENTER, 2, 1, LV_GRID_ALIGN_END, 0, 2);
  // lv_obj_set_grid_cell(extrude_btn.get_container(), LV_GRID_ALIGN_CENTER, 2, 1, LV_GRID_ALIGN_START, 2, 2);
  // lv_obj_set_grid_cell(back_btn.get_container(), LV_GRID_ALIGN_END, 2, 1, LV_GRID_ALIGN_END, 2, 2);
  

  ws.register_notify_update(this);    
}

ExtruderPanel::~ExtruderPanel() {
  if (clock_timer != NULL) {
    lv_timer_del(clock_timer);
    clock_timer = NULL;
  }

  if (panel_cont != NULL) {
    lv_obj_del(panel_cont);
    panel_cont = NULL;
  }
}

void ExtruderPanel::foreground() {
  lv_obj_move_foreground(panel_cont);
}

void ExtruderPanel::update_clock() {
  const std::time_t now = std::time(nullptr);
  const std::tm local_time = *std::localtime(&now);
  char time_text[6] = {};
  std::strftime(time_text, sizeof(time_text), "%H:%M", &local_time);
  lv_label_set_text(time_label, time_text);
}

void ExtruderPanel::show_manual_filament_change() {
  // Use PowerScreen's native prompt protocol so the dialog is rendered by the
  // printer UI and remains compatible with the existing prompt handling.
  ws.gcode_script(
    "RESPOND TYPE=command MSG=\"action:prompt_begin MANUAL FILAMENT CHANGE\"\n"
    "RESPOND TYPE=command MSG=\"action:prompt_footer_button UNLOAD|SDK_UNLOAD_FILAMENT|warning\"\n"
    "RESPOND TYPE=command MSG=\"action:prompt_footer_button LOAD|SDK_LOAD_FILAMENT|primary\"\n"
    "RESPOND TYPE=command MSG=\"action:prompt_footer_button RESUME|RESUME|success\"\n"
    "RESPOND TYPE=command MSG=\"action:prompt_footer_button STOP|CANCEL_PRINT|error\"\n"
    "RESPOND TYPE=command MSG=\"action:prompt_footer_button CLOSE|RESPOND TYPE=command MSG='action:prompt_end'|secondary\"\n"
    "RESPOND TYPE=command MSG=\"action:prompt_show\"");
}

void ExtruderPanel::enable_spoolman() {
  spoolman_btn.enable();
}

void ExtruderPanel::consume(json& j) {
  std::lock_guard<std::mutex> lock(lv_lock);
  auto target_value = j["/params/0/extruder/target"_json_pointer];
  if (!target_value.is_null()) {
    int target = target_value.template get<int>();
    extruder_temp.update_target(target);
  }
  
  auto temp_value = j["/params/0/extruder/temperature"_json_pointer];
  if (!temp_value.is_null()) {   
    int value = temp_value.template get<int>();
    extruder_temp.update_value(value);
  }
}

void ExtruderPanel::handle_callback(lv_event_t *e) {
  spdlog::trace("handling extruder panel callback");
  if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
    lv_obj_t *selector = lv_event_get_target(e);
    uint32_t idx = lv_btnmatrix_get_selected_btn(selector);
    const char * v = lv_btnmatrix_get_btn_text(selector, idx);

    if (selector == temp_selector.get_selector()) {
      temp_selector.set_selected_idx(idx);
    }

    if (selector == length_selector.get_selector()) {
      length_selector.set_selected_idx(idx);
    }

    if (selector == speed_selector.get_selector()) {
      speed_selector.set_selected_idx(idx);
    }

    spdlog::trace("selector {} {} {}, {} {} {}", fmt::ptr(selector), idx, v,
		  fmt::ptr(temp_selector.get_selector()),
		  fmt::ptr(length_selector.get_selector()),
		  fmt::ptr(speed_selector.get_selector()));
    
  } else if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    lv_obj_t *btn = lv_event_get_current_target(e);

    if (btn == back_btn.get_container()) {
      lv_obj_move_background(panel_cont);
    }

    if (btn == extrude_btn.get_container()) {
      const char * temp = lv_btnmatrix_get_btn_text(temp_selector.get_selector(),
						   temp_selector.get_selected_idx());
      const char * len = lv_btnmatrix_get_btn_text(length_selector.get_selector(),
						   length_selector.get_selected_idx());
      const char *speed = lv_btnmatrix_get_btn_text(speed_selector.get_selector(),
						    speed_selector.get_selected_idx());
      ws.gcode_script(fmt::format("M109 S{}\nM83\nG1 E{} F{}", temp, len, std::stoi(speed) * 60));
    }

    if (btn == retract_btn.get_container()) {
      const char * temp = lv_btnmatrix_get_btn_text(temp_selector.get_selector(),
						   temp_selector.get_selected_idx());
      const char * len = lv_btnmatrix_get_btn_text(length_selector.get_selector(),
						   length_selector.get_selected_idx());
      const char *speed = lv_btnmatrix_get_btn_text(speed_selector.get_selector(),
						    speed_selector.get_selected_idx());
      ws.gcode_script(fmt::format("M109 S{}\nM83\nG1 E-{} F{}", temp, len, std::stoi(speed) * 60));
    }

    if (btn == unload_btn.get_container()) {
      if (unload_filament_macro == "_POWERSCREEN_QUIT_MATERIAL") {
        const char *temp = lv_btnmatrix_get_btn_text(temp_selector.get_selector(),
                                                     temp_selector.get_selected_idx());
        ws.gcode_script(fmt::format("{} EXTRUDER_TEMP={}", unload_filament_macro, temp));
      } else {
        ws.gcode_script(unload_filament_macro);
      }
    }

    if (btn == manual_change_btn.get_container()) {
      show_manual_filament_change();
    }

    if (btn == load_btn.get_container()) {
      if (load_filament_macro == "_POWERSCREEN_LOAD_MATERIAL") {
        const char *temp = lv_btnmatrix_get_btn_text(temp_selector.get_selector(),
                                                     temp_selector.get_selected_idx());
        const char *len = lv_btnmatrix_get_btn_text(length_selector.get_selector(),
                                                    length_selector.get_selected_idx());
        ws.gcode_script(fmt::format("{} EXTRUDER_TEMP={} EXTRUDE_LEN={}", load_filament_macro, temp, len));
      } else {
        ws.gcode_script(load_filament_macro);
      }
    }

    if (btn == cooldown_btn.get_container()) {
      ws.gcode_script(cooldown_macro);
    }

    if (btn == spoolman_btn.get_container()) {
      spoolman_panel.foreground();
    }
  }
}
