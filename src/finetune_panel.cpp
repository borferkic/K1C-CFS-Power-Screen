#include "finetune_panel.h"
#include "powerui.h"
#include "button_container.h"
#include "state.h"
#include "spdlog/spdlog.h"
#include "config.h"

#include <algorithm>
#include <ctime>

LV_IMG_DECLARE(home_z);
LV_IMG_DECLARE(z_closer);
LV_IMG_DECLARE(z_farther);
LV_IMG_DECLARE(pa_plus_img);
LV_IMG_DECLARE(pa_minus_img);
LV_IMG_DECLARE(refresh_img);
LV_IMG_DECLARE(speed_up_img);
LV_IMG_DECLARE(speed_down_img);
LV_IMG_DECLARE(flow_up_img);
LV_IMG_DECLARE(flow_down_img);
LV_IMG_DECLARE(back);

constexpr uint32_t CREALITY_GREEN = powerui::COLOR_ACCENT;
constexpr uint32_t CREALITY_GREEN_PRESSED = powerui::COLOR_ACCENT_PRESSED;

FineTunePanel::FineTunePanel(KWebSocketClient &websocket_client, std::mutex &l)
  : NotifyConsumer(l)
  , ws(websocket_client)
  , panel_cont(lv_obj_create(lv_scr_act()))
  , title_bar(lv_obj_create(panel_cont))
  , title_label(lv_label_create(title_bar))
  , time_label(lv_label_create(title_bar))
  , clock_timer(NULL)
  , values_cont(lv_obj_create(panel_cont))
  , zreset_btn(panel_cont, NULL, "Reset", &FineTunePanel::_handle_zoffset, this)
  , zup_btn(panel_cont, NULL, "+", &FineTunePanel::_handle_zoffset, this)
  , zdown_btn(panel_cont, NULL, "-", &FineTunePanel::_handle_zoffset, this)
  , pareset_btn(panel_cont, NULL, "Reset", &FineTunePanel::_handle_pa, this)
  , paup_btn(panel_cont, NULL, "+", &FineTunePanel::_handle_pa, this)
  , padown_btn(panel_cont, NULL, "-", &FineTunePanel::_handle_pa, this)
  , speed_reset_btn(panel_cont, NULL, "Reset", &FineTunePanel::_handle_speed, this)    
  , speed_up_btn(panel_cont, NULL, "+", &FineTunePanel::_handle_speed, this)
  , speed_down_btn(panel_cont, NULL, "-", &FineTunePanel::_handle_speed, this)
  , flow_reset_btn(panel_cont, NULL, "Reset", &FineTunePanel::_handle_flow, this)
  , flow_up_btn(panel_cont, NULL, "+", &FineTunePanel::_handle_flow, this)
  , flow_down_btn(panel_cont, NULL, "-", &FineTunePanel::_handle_flow, this)
  , back_btn(panel_cont, &back, "Back", &FineTunePanel::_handle_callback, this)
  , zoffset_selector(panel_cont, "Z (mm) - PA (mm/s)",
		     {"0.01", "0.025", "0.05", "0.10", ""}, 0, 30, 15, &FineTunePanel::_handle_callback, this)
  , multipler_selector(panel_cont, "Multipler Step (%)",
		       {"1", "5", "10", "25", ""}, 0, 40, 15, &FineTunePanel::_handle_callback, this)
  , z_offset(values_cont, &home_z, 150, 100, 25, "0.0 mm")
  , pa(values_cont, &pa_plus_img, 150, 100, 25, "0.000")
  , speed_factor(values_cont, &speed_up_img, 150, 100, 25 ,"100%")
  , flow_factor(values_cont, &flow_up_img, 150, 100, 25, "100%")
{
  lv_obj_move_background(panel_cont);
  
  lv_obj_set_size(panel_cont, LV_PCT(100), LV_PCT(100));
  lv_obj_clear_flag(panel_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(panel_cont, 0, LV_PART_MAIN);

  lv_obj_add_flag(title_bar, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(title_bar, LV_PCT(100), 32);
  lv_obj_set_pos(title_bar, 0, 0);
  lv_obj_set_style_pad_all(title_bar, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_color(title_bar, lv_color_hex(powerui::COLOR_CARD), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(title_bar, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(title_bar, 0, LV_PART_MAIN);

  lv_label_set_text(title_label, "TUNE");
  lv_obj_set_width(title_label, LV_PCT(100));
  lv_label_set_long_mode(title_label, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_align(title_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_style_text_color(title_label, lv_color_hex(powerui::COLOR_WHITE), LV_PART_MAIN);
  lv_obj_set_style_text_font(title_label, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_align(title_label, LV_ALIGN_CENTER, 0, 0);

  lv_obj_set_width(time_label, LV_SIZE_CONTENT);
  lv_obj_set_style_text_color(time_label, lv_color_hex(powerui::COLOR_WHITE), LV_PART_MAIN);
  lv_obj_set_style_text_font(time_label, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_align(time_label, LV_ALIGN_RIGHT_MID, -10, 0);
  update_clock();
  clock_timer = lv_timer_create(&FineTunePanel::_update_clock_cb, 1000, this);

  // ---- PowerUI layout: four metric cards (value, up / down, reset) and two step selectors.
  using namespace powerui;
  lv_obj_add_flag(title_bar, LV_OBJ_FLAG_HIDDEN);  // the main title bar shows "Fine Tune"
  lv_obj_set_style_bg_color(panel_cont, lv_color_hex(powerui::COLOR_BG), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(panel_cont, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(panel_cont, 0, LV_PART_MAIN);
  lv_obj_add_flag(values_cont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(back_btn.get_container(), LV_OBJ_FLAG_HIDDEN);

  struct Metric {
    ImageLabel *value;
    ButtonContainer *up;
    ButtonContainer *down;
    ButtonContainer *reset;
  };
  Metric metrics[4] = {{&z_offset, &zup_btn, &zdown_btn, &zreset_btn},
                       {&pa, &paup_btn, &padown_btn, &pareset_btn},
                       {&speed_factor, &speed_up_btn, &speed_down_btn, &speed_reset_btn},
                       {&flow_factor, &flow_up_btn, &flow_down_btn, &flow_reset_btn}};
  struct Header { const lv_img_dsc_t *icon; const char *title; };
  const Header headers[4] = {{&home_z, "Z offset"}, {&pa_plus_img, "Pressure advance"}, {&speed_up_img, "Speed"}, {&flow_up_img, "Flow"}};
  for (int c = 0; c < 4; ++c) {
    const int x0 = 12 + c * 181;
    lv_obj_t *metric_card = card(panel_cont, x0, 12, 169, 270);
    lv_obj_move_to_index(metric_card, 0);

    lv_obj_t *header_icon = icon(metric_card, headers[c].icon, 20, lv_color_hex(powerui::COLOR_MUTED));
    lv_obj_set_pos(header_icon, px(14), px(14));
    lv_obj_t *header_title = label(metric_card, headers[c].title, &lv_font_montserrat_12, lv_color_hex(powerui::COLOR_MUTED));
    lv_obj_set_pos(header_title, px(40), px(17));

    // Value: only the number (the ImageLabel icon stays hidden).
    lv_obj_t *value = metrics[c].value->get_container();
    lv_obj_set_parent(value, metric_card);
    lv_obj_set_size(value, px(145), px(40));
    lv_obj_set_pos(value, px(11), px(40));
    lv_obj_set_style_bg_opa(value, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(value, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(value, 0, LV_PART_MAIN);
    lv_obj_t *value_icon = lv_obj_get_child(value, 0);
    if (value_icon != NULL) {
      lv_obj_add_flag(value_icon, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_t *value_label = lv_obj_get_child(value, 1);
    if (value_label != NULL) {
      lv_obj_set_style_text_font(value_label, &lv_font_montserrat_24, LV_PART_MAIN);
      lv_obj_set_style_text_color(value_label, lv_color_hex(powerui::COLOR_FG), LV_PART_MAIN);
      lv_obj_set_style_text_align(value_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
      lv_obj_align(value_label, LV_ALIGN_LEFT_MID, px(3), 0);
    }

    // + and - (outlined), Reset (soft green): plain text buttons like the reference.
    const struct { ButtonContainer *button; int y; int h; ButtonKind kind; const lv_font_t *font; } buttons[3] = {
      {metrics[c].up, 96, 52, ButtonKind::Outline, &lv_font_montserrat_24},
      {metrics[c].down, 156, 52, ButtonKind::Outline, &lv_font_montserrat_24},
      {metrics[c].reset, 216, 42, ButtonKind::Soft, &lv_font_montserrat_14}};
    for (const auto &entry : buttons) {
      entry.button->set_fixed_size(px(145), px(entry.h));
      style_button(entry.button->get_container(), entry.button->get_button(), entry.kind);
      lv_obj_set_pos(entry.button->get_container(), px(x0 + 12), px(12 + entry.y));
      lv_obj_t *button_label = lv_obj_get_child(entry.button->get_container(), 1);
      if (button_label != NULL) {
        lv_obj_set_style_text_font(button_label, entry.font, LV_PART_MAIN);
      }
    }
  }

  // Step selectors in two cards at the bottom.
  struct StepCard { Selector *selector; int x; const char *title; };
  StepCard steps[2] = {{&zoffset_selector, 12, "Z (mm) / PA step"}, {&multipler_selector, 374, "Multiplier step (%)"}};
  for (const auto &step : steps) {
    lv_obj_t *step_card = card(panel_cont, step.x, 294, 350, 134);
    lv_obj_move_to_index(step_card, 0);
    lv_obj_t *selector = step.selector->get_container();
    lv_obj_set_parent(selector, step_card);
    lv_obj_set_size(selector, px(322), px(100));
    lv_obj_set_pos(selector, px(14), px(12));
    lv_obj_set_flex_align(selector, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(selector, px(26), 0);
    lv_obj_set_style_bg_opa(selector, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(selector, 0, LV_PART_MAIN);
    lv_obj_t *selector_label = step.selector->get_label();
    lv_label_set_text(selector_label, step.title);
    lv_obj_set_style_text_align(selector_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_color(selector_label, lv_color_hex(powerui::COLOR_MUTED), 0);
    lv_obj_set_style_text_font(selector_label, &lv_font_montserrat_14, 0);
    lv_obj_t *btnm = step.selector->get_selector();
    lv_obj_set_size(btnm, px(322), px(56));
    style_segmented(btnm);
  }

  ws.register_notify_update(this);
}

FineTunePanel::~FineTunePanel() {
  if (clock_timer != NULL) {
    lv_timer_del(clock_timer);
    clock_timer = NULL;
  }

  if (panel_cont != NULL) {
    lv_obj_del(panel_cont);
    panel_cont = NULL;
  }
  ws.unregister_notify_update(this);
}

void FineTunePanel::update_clock() {
  const std::time_t now = std::time(nullptr);
  const std::tm local_time = *std::localtime(&now);
  char time_text[6] = {};
  std::strftime(time_text, sizeof(time_text), "%H:%M", &local_time);
  lv_label_set_text(time_label, time_text);
}

void FineTunePanel::foreground() {
  auto v = State::get_instance()->get_data(
		"/printer_state/gcode_move/homing_origin/2"_json_pointer);
  if (!v.is_null()) {
    z_offset.update_label(fmt::format("{:.3f} mm", v.template get<double>()).c_str());
  }

  v = State::get_instance()->get_data(
		"/printer_state/extruder/pressure_advance"_json_pointer);
  if (!v.is_null()) {
    pa.update_label(fmt::format("{:.3f}", v.template get<double>()).c_str());
  }

  v = State::get_instance()->get_data(
		"/printer_state/gcode_move/speed_factor"_json_pointer);
  if (!v.is_null()) {
    speed_factor.update_label(fmt::format("{} %",
	   static_cast<int>(v.template get<double>() * 100)).c_str());
  }

  v = State::get_instance()->get_data(
		"/printer_state/gcode_move/extrude_factor"_json_pointer);
  if (!v.is_null()) {
    flow_factor.update_label(fmt::format("{} %",
	   static_cast<int>(v.template get<double>() * 100)).c_str());
  }

  lv_obj_move_foreground(panel_cont);
  lv_obj_add_flag(back_btn.get_container(), LV_OBJ_FLAG_HIDDEN);  // Back lives in the title bar
  powerui::overlay_open("Fine Tune", [this]() { lv_obj_move_background(panel_cont); });
}

void FineTunePanel::consume(json &j) {
  std::lock_guard<std::mutex> lock(lv_lock);
  auto v = j["/params/0/gcode_move/homing_origin/2"_json_pointer];
  if (!v.is_null()) {
    z_offset.update_label(fmt::format("{:.3f} mm", v.template get<double>()).c_str());
  }

  v = j["/params/0/extruder/pressure_advance"_json_pointer];
  if (!v.is_null()) {
    pa.update_label(fmt::format("{:.3f}", v.template get<double>()).c_str());
  }

  v = j["/params/0/gcode_move/speed_factor"_json_pointer];
  if (!v.is_null()) {
    speed_factor.update_label(fmt::format("{} %",
	   static_cast<int>(v.template get<double>() * 100)).c_str());
  }

  v = j["/params/0/gcode_move/extrude_factor"_json_pointer];
  if (!v.is_null()) {
    flow_factor.update_label(fmt::format("{} %",
	   static_cast<int>(v.template get<double>() * 100)).c_str());
  }
}

void FineTunePanel::handle_callback(lv_event_t *e) {
  spdlog::trace("fine tune btn callback");
  if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
    lv_obj_t *selector = lv_event_get_target(e);
    uint32_t idx = lv_btnmatrix_get_selected_btn(selector);

    if (selector == zoffset_selector.get_selector()) {
      zoffset_selector.set_selected_idx(idx);
    }

    if (selector == multipler_selector.get_selector()) {
      multipler_selector.set_selected_idx(idx);
    }
  }
  
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    lv_obj_t *btn = lv_event_get_current_target(e);
    if (btn == back_btn.get_container()) {
      lv_obj_move_background(panel_cont);
    }
  }
}

void FineTunePanel::handle_zoffset(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    lv_obj_t *btn = lv_event_get_current_target(e);

    if (btn == zreset_btn.get_container()) {
      spdlog::trace("clicked zoffset reset");
      ws.gcode_script("SET_GCODE_OFFSET Z=0 MOVE=1");
    } else {
      const char * step = lv_btnmatrix_get_btn_text(zoffset_selector.get_selector(),
						    zoffset_selector.get_selected_idx());
      spdlog::trace("clicked z {}", step);
      ws.gcode_script(fmt::format("SET_GCODE_OFFSET Z_ADJUST={}{} MOVE=1",
				  btn == zup_btn.get_container() ? "+" : "-",
				  step));
    }
  }
}

void FineTunePanel::handle_pa(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    lv_obj_t *btn = lv_event_get_current_target(e);

    if (btn == pareset_btn.get_container()) {
      spdlog::trace("clicked pa reset");
      auto v = State::get_instance()->get_data(
		     "/printer_state/configfile/settings/extruder/pressure_advance"_json_pointer);
      if (!v.is_null()) {
	ws.gcode_script(fmt::format("SET_PRESSURE_ADVANCE ADVANCE={}", v.template get<double>()));
      }
    } else {

      auto cur_pa = State::get_instance()
	->get_data("/printer_state/extruder/pressure_advance"_json_pointer);
      if (!cur_pa.is_null()) {
	const char * step = lv_btnmatrix_get_btn_text(zoffset_selector.get_selector(),
						      zoffset_selector.get_selected_idx());
	
	double direction = btn == paup_btn.get_container() ? std::stod(step) : -std::stod(step);
	double new_pa = cur_pa.template get<double>() + direction;
	new_pa = new_pa < 0 ? 0 : new_pa;
	ws.gcode_script(fmt::format("SET_PRESSURE_ADVANCE ADVANCE={}", new_pa));
      }
    }
  }
}

void FineTunePanel::handle_speed(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    lv_obj_t *btn = lv_event_get_current_target(e);
    if (btn == speed_reset_btn.get_container()) {
      spdlog::trace("speed reset");
      ws.gcode_script("M220 S100");
    } else {
      auto spd_factor = State::get_instance()
	->get_data("/printer_state/gcode_move/speed_factor"_json_pointer);
      if (!spd_factor.is_null()) {
	const char * step = lv_btnmatrix_get_btn_text(multipler_selector.get_selector(),
						      multipler_selector.get_selected_idx());

	int32_t direction = btn == speed_up_btn.get_container() ? std::stoi(step) : -std::stoi(step);
	int32_t new_speed = static_cast<int32_t>(spd_factor.template get<double>() * 100 + direction);
	new_speed = std::max(new_speed, 1);
	spdlog::trace("speed step {}, {}", direction, new_speed);
	ws.gcode_script(fmt::format("M220 S{}", new_speed));
      }
    }
  }
}

void FineTunePanel::handle_flow(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    lv_obj_t *btn = lv_event_get_current_target(e);
    if (btn == flow_reset_btn.get_container()) {
      spdlog::trace("flow reset");
      ws.gcode_script("M221 S100");
    } else {
      auto extrude_factor = State::get_instance()
	->get_data("/printer_state/gcode_move/extrude_factor"_json_pointer);
      if (!extrude_factor.is_null()) {
	const char * step = lv_btnmatrix_get_btn_text(multipler_selector.get_selector(),
						      multipler_selector.get_selected_idx());

	int32_t direction = btn == flow_up_btn.get_container() ? std::stoi(step) : -std::stoi(step);
	
	int32_t new_flow = static_cast<int32_t>(extrude_factor.template get<double>() * 100 + direction);
	new_flow = std::max(new_flow, 1);
	spdlog::trace("flow step {}, {}", direction, new_flow);
	ws.gcode_script(fmt::format("M221 S{}", new_flow));
      }
    }
  }
}
