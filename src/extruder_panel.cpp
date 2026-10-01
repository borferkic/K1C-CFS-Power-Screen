#include "extruder_panel.h"
#include "powerui.h"
#include "state.h"
#include "config.h"
#include "spdlog/spdlog.h"

#include <limits>

LV_IMG_DECLARE(back);
LV_IMG_DECLARE(ui_cfs_img);
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
  lv_obj_set_style_bg_color(title_bar, lv_color_hex(0x171717), 0);
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

  // PowerUI layout for the overlay area (736 x 440): left rail, three option cards, right rail.
  using namespace powerui;
  lv_obj_set_style_radius(panel_cont, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_color(panel_cont, lv_color_hex(COLOR_BG), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(panel_cont, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(panel_cont, 0, LV_PART_MAIN);

  // The main title bar replaces the panel's own one; the clock timer keeps running on the hidden label.
  lv_obj_add_flag(title_bar, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(leftside_btns_cont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(rightside_btns_cont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(panel_cont, LV_OBJ_FLAG_SCROLLABLE);

  spoolman_btn.disable();

  auto put = [this](ButtonContainer &button, int x, int y, int w, int h) {
    lv_obj_set_parent(button.get_container(), panel_cont);
    button.set_fixed_size(px(w), px(h));
    lv_obj_set_pos(button.get_container(), px(x), px(y));
  };

  // Left rail (x 12, 140 wide, 95 high with 12 px gaps).
  put(manual_change_btn, 12, 12, 140, 95);
  put(load_btn, 12, 119, 140, 95);
  put(unload_btn, 12, 226, 140, 95);
  put(cooldown_btn, 12, 333, 140, 95);
  // Right rail.
  put(spoolman_btn, 584, 12, 140, 95);
  put(extrude_btn, 584, 119, 140, 95);
  put(retract_btn, 584, 226, 140, 95);
  put(back_btn, 594, 358, 120, 44);

  for (ButtonContainer *button : {&manual_change_btn, &load_btn, &unload_btn, &spoolman_btn, &retract_btn}) {
    style_button(button->get_container(), button->get_button(), ButtonKind::Outline);
  }
  style_button(extrude_btn.get_container(), extrude_btn.get_button(), ButtonKind::Soft);
  style_button(cooldown_btn.get_container(), cooldown_btn.get_button(), ButtonKind::Outline);
  cooldown_btn.set_image_color(lv_color_hex(COLOR_CHAMBER));
  lv_obj_set_style_text_color(cooldown_btn.get_container(), lv_color_hex(COLOR_CHAMBER), LV_PART_MAIN);

  // CFS: icon (four spools) to the left of the label; red while the CFS is not available.
  spoolman_btn.set_disabled_text_color(lv_color_hex(COLOR_DESTRUCTIVE));
  spoolman_icon = powerui::icon(spoolman_btn.get_container(), &ui_cfs_img, 26, lv_color_hex(COLOR_DESTRUCTIVE));
  lv_obj_align(spoolman_icon, LV_ALIGN_LEFT_MID, 12, 0);

  // Option cards: title on top and a segmented selector below.
  struct CardSpec { Selector *selector; const char *title; int y; int h; };
  const CardSpec specs[3] = {{&speed_selector, "Extrude speed (mm/s)", 12, 130},
                             {&length_selector, "Extrude length (mm)", 154, 130},
                             {&temp_selector, "Extruder temperature (°C)", 296, 132}};
  lv_obj_t *temp_card = NULL;
  for (const CardSpec &spec : specs) {
    lv_obj_t *option_card = card(panel_cont, 164, spec.y, 408, spec.h);
    lv_obj_t *title = label(option_card, spec.title, &lv_font_montserrat_14, lv_color_hex(COLOR_MUTED));
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, px(15), px(14));

    lv_obj_t *selector_cont = spec.selector->get_container();
    lv_obj_set_parent(selector_cont, option_card);
    lv_obj_add_flag(spec.selector->get_label(), LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_size(selector_cont, px(374), px(52));
    lv_obj_align(selector_cont, LV_ALIGN_BOTTOM_MID, 0, -px(16));
    lv_obj_set_size(spec.selector->get_selector(), px(374), px(52));
    style_segmented(spec.selector->get_selector());
    if (spec.selector == &temp_selector) {
      temp_card = option_card;
    }
  }

  // Current extruder temperature next to the temperature card title.
  lv_obj_set_parent(extruder_temp.get_sensor(), temp_card);
  lv_obj_clear_flag(extruder_temp.get_sensor(), LV_OBJ_FLAG_FLOATING);
  lv_obj_set_size(extruder_temp.get_sensor(), px(120), px(32));
  extruder_temp.set_current_only(px(70), px(5));
  extruder_temp.set_image_zoom(100);
  lv_obj_align(extruder_temp.get_sensor(), LV_ALIGN_TOP_RIGHT, -px(10), px(6));

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
  // The menu is defined once in powerscreen_cmd.cfg, so this button and the
  // M600 macro always show the same prompt.
  ws.gcode_script("_PS_MANUAL_CHANGE_PROMPT");
}

void ExtruderPanel::enable_spoolman() {
  spoolman_btn.enable();
  powerui::icon_set_color(spoolman_icon, lv_color_hex(0xFAFAFA));
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
      if (unload_filament_macro == "_PS_QUIT_MATERIAL") {
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
      if (load_filament_macro == "_PS_LOAD_MATERIAL") {
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
