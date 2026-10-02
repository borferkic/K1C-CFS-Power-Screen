#include "belts_calibration_panel.h"
#include "powerui.h"
#include "utils.h"
#include "config.h"
#include "spdlog/spdlog.h"

LV_IMG_DECLARE(ui_icon_play);
LV_IMG_DECLARE(ui_icon_activity);
LV_IMG_DECLARE(ui_icon_estop);
LV_IMG_DECLARE(emergency);
LV_IMG_DECLARE(back);

namespace {
// Graph area inside the left card (design px, overlay coordinates).
constexpr int GRAPH_X = 30;
constexpr int GRAPH_Y = 90;
constexpr int GRAPH_W = 404;
constexpr int GRAPH_H = 250;
}

#define BELTS_PNG "belts_calibration.png"

std::vector<std::string> BeltsCalibrationPanel::axes = {
  "x",
  "y",
  "a",
  "b"
};

BeltsCalibrationPanel::BeltsCalibrationPanel(KWebSocketClient &c, std::mutex &l)
  : ws(c)
  , lv_lock(l)
  , cont(lv_obj_create(lv_scr_act()))
  , graph_cont(NULL)
  , graph(NULL)
  , spinner(NULL)
  , excite_control(NULL)
  , excite_slider(NULL)
  , excite_label(NULL)
  , excite_dd(NULL)
  , graph_hint(NULL)
  , calibrate_btn(NULL)
  , excite_btn(NULL)
  , stop_btn(NULL)
  , emergency_btn(cont, &emergency, "Stop", &BeltsCalibrationPanel::_handle_callback, this,
		  "Do you want to emergency stop?",
		  [&c]() {
		    spdlog::debug("emergency stop pressed");
		    c.send_jsonrpc("printer.emergency_stop");
		  })
  , back_btn(cont, &back, "Back", &BeltsCalibrationPanel::_handle_callback, this)
  , image_fullsized(false)
{
  using namespace powerui;
  lv_obj_move_background(cont);
  style_overlay_root(cont);
  lv_obj_add_flag(emergency_btn.get_container(), LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(back_btn.get_container(), LV_OBJ_FLAG_HIDDEN);

  // Left card: belt resonance graph.
  lv_obj_t *graph_card = card(cont, 12, 12, 452, 416);
  lv_obj_t *title = label(graph_card, "Belt resonance", &lv_font_montserrat_16, lv_color_hex(COLOR_FG));
  lv_obj_set_pos(title, px(20), px(16));
  lv_obj_t *subtitle = label(graph_card, "Compares the two belts of the CoreXY", &lv_font_montserrat_12, lv_color_hex(COLOR_MUTED));
  lv_obj_set_pos(subtitle, px(20), px(42));
  lv_obj_t *footer = label(graph_card, "Run Shake Belts to refresh the graph. Tap the graph to enlarge it.", &lv_font_montserrat_12,
			   lv_color_hex(COLOR_MUTED));
  lv_obj_align(footer, LV_ALIGN_BOTTOM_LEFT, px(20), -px(14));

  graph_cont = plain(cont);
  lv_obj_add_flag(graph_cont, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_pos(graph_cont, px(GRAPH_X), px(GRAPH_Y));
  lv_obj_set_size(graph_cont, px(GRAPH_W), px(GRAPH_H));
  lv_obj_set_style_radius(graph_cont, px(10), 0);
  lv_obj_set_style_bg_color(graph_cont, lv_color_hex(COLOR_BG), 0);
  lv_obj_set_style_bg_opa(graph_cont, LV_OPA_COVER, 0);
  lv_obj_set_style_clip_corner(graph_cont, true, 0);
  lv_obj_add_event_cb(graph_cont, &BeltsCalibrationPanel::_handle_image_clicked, LV_EVENT_CLICKED, this);

  graph = lv_img_create(graph_cont);
  lv_obj_clear_flag(graph, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(graph, LV_OBJ_FLAG_HIDDEN);
  lv_obj_center(graph);

  graph_hint = label(graph_cont, "No graph yet", &lv_font_montserrat_14, lv_color_hex(COLOR_MUTED));
  lv_obj_center(graph_hint);

  spinner = lv_spinner_create(graph_cont, 1000, 60);
  lv_obj_add_flag(spinner, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_size(spinner, px(60), px(60));
  lv_obj_center(spinner);

  // Right card: excite controls.
  lv_obj_t *excite_card = card(cont, 476, 12, 248, 238);
  excite_control = excite_card;
  lv_obj_t *excite_title = label(excite_card, "Excite", &lv_font_montserrat_16, lv_color_hex(COLOR_FG));
  lv_obj_set_pos(excite_title, px(20), px(16));
  lv_obj_t *freq_title = label(excite_card, "Frequency", &lv_font_montserrat_12, lv_color_hex(COLOR_MUTED));
  lv_obj_set_pos(freq_title, px(20), px(56));
  excite_label = label(excite_card, "1 hz", &lv_font_montserrat_20, lv_color_hex(COLOR_FG));
  lv_obj_align(excite_label, LV_ALIGN_TOP_RIGHT, -px(20), px(48));

  excite_slider = lv_slider_create(excite_card);
  lv_obj_set_size(excite_slider, px(208), px(14));
  lv_obj_set_pos(excite_slider, px(20), px(92));
  lv_slider_set_range(excite_slider, 10, 1400);
  style_slider(excite_slider);
  lv_obj_add_event_cb(excite_slider, &BeltsCalibrationPanel::_handle_update_slider, LV_EVENT_VALUE_CHANGED, this);

  lv_obj_t *axis_title = label(excite_card, "Axis", &lv_font_montserrat_12, lv_color_hex(COLOR_MUTED));
  lv_obj_set_pos(axis_title, px(20), px(142));
  excite_dd = lv_dropdown_create(excite_card);
  lv_dropdown_set_options(excite_dd, fmt::format("{}", fmt::join(axes, "\n")).c_str());
  lv_obj_set_size(excite_dd, px(208), px(40));
  lv_obj_set_pos(excite_dd, px(20), px(166));
  style_select(excite_dd);

  // Actions.
  calibrate_btn = action_button(cont, &ui_icon_play, "Shake Belts", ActionKind::Primary, 476, 262, 248, 52,
				&BeltsCalibrationPanel::_handle_callback, this);
  excite_btn = action_button(cont, &ui_icon_activity, "Excite", ActionKind::Outline, 476, 322, 248, 48,
			     &BeltsCalibrationPanel::_handle_callback, this);
  stop_btn = action_button(cont, &ui_icon_estop, "Stop", ActionKind::Destructive, 476, 378, 248, 50,
			   &BeltsCalibrationPanel::_handle_callback, this);

  ws.register_method_callback("notify_gcode_response",
			      "BeltsCalibrationPanel",
			      [this](json& d) { this->handle_macro_response(d); });
  
}

BeltsCalibrationPanel::~BeltsCalibrationPanel() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

void BeltsCalibrationPanel::foreground() {
  lv_obj_move_foreground(cont);
  lv_obj_add_flag(back_btn.get_container(), LV_OBJ_FLAG_HIDDEN);  // Back lives in the title bar
  powerui::overlay_open("Belts / Shake", [this]() { lv_obj_move_background(cont); });
}

void BeltsCalibrationPanel::handle_callback(lv_event_t *event) {
  lv_obj_t *btn = lv_event_get_current_target(event);
  if (btn == calibrate_btn) {
    auto config_root = KUtils::get_root_path("config");
    auto png_path = fmt::format("{}/{}", config_root.length() > 0 ? config_root : "/tmp" , BELTS_PNG);

    if (!KUtils::is_homed()) {
      ws.gcode_script("G28");
    }

    auto screen_width = (double)powerui::overlay_width_px() / 100.0;
    auto screen_height = (double)powerui::overlay_height_px() / 100.0;
    ws.gcode_script(fmt::format("PS_BELTS_SHAPER_CALIBRATION PNG_OUT_PATH={} PNG_WIDTH={} PNG_HEIGHT={}",
				png_path, screen_width, screen_height));

    // ws.gcode_script(fmt::format("PS_BELTS_SHAPER_CALIBRATION PNG_OUT_PATH={} PNG_WIDTH={} PNG_HEIGHT={} FREQ_START=5 FREQ_END=10",
    // 				png_path, screen_width, screen_height));
    

    lv_obj_add_flag(graph, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(graph_hint, LV_OBJ_FLAG_HIDDEN);
    lv_img_set_src(graph, NULL);    
    lv_obj_invalidate(graph);
    lv_obj_clear_flag(spinner, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(spinner);

  } else if (btn == excite_btn) {
    double excite_hz = (double)lv_slider_get_value(excite_slider) / 10.0 + 1; // [x-1, x+1]
    char excite_buf[10];
    lv_dropdown_get_selected_str(excite_dd, excite_buf, sizeof(excite_buf));

    if (!KUtils::is_homed()) {
      ws.gcode_script("G28");
    }
    ws.gcode_script(fmt::format("PS_EXCITATE_AXIS_AT_FREQ FREQUENCY={} AXIS={}", excite_hz, excite_buf));

  } else if (btn == back_btn.get_container()) {
    lv_obj_move_background(cont);
  } else if (btn == stop_btn || btn == emergency_btn.get_container()) {
    emergency_stop();
  }
}

void BeltsCalibrationPanel::emergency_stop() {
  Config *conf = Config::get_instance();
  auto estop = conf->get_json("/prompt_emergency_stop");
  if (!estop.is_null() && estop.template get<bool>()) {
    emergency_btn.handle_prompt();
  } else {
    ws.send_jsonrpc("printer.emergency_stop");
  }
}

// Scale the graph PNG to the width of its box (small card) or of the whole panel (enlarged).
void BeltsCalibrationPanel::fit_graph() {
  lv_img_header_t header;
  const void *src = lv_img_get_src(graph);
  if (src == NULL || lv_img_decoder_get_info(src, &header) != LV_RES_OK || header.w == 0) {
    return;
  }
  const lv_coord_t box_w = lv_obj_get_width(graph_cont);
  lv_img_set_zoom(graph, static_cast<uint16_t>(256 * box_w / header.w));
  lv_obj_center(graph);
}

void BeltsCalibrationPanel::handle_image_clicked(lv_event_t *e) {
  const lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_CLICKED) {
    lv_obj_t *clicked = lv_event_get_target(e);

    if (clicked == graph_cont) {
      if (image_fullsized) {
	lv_obj_set_pos(graph_cont, powerui::px(GRAPH_X), powerui::px(GRAPH_Y));
	lv_obj_set_size(graph_cont, powerui::px(GRAPH_W), powerui::px(GRAPH_H));
	lv_obj_clear_flag(graph_cont, LV_OBJ_FLAG_FLOATING);
      } else {
	lv_obj_add_flag(graph_cont, LV_OBJ_FLAG_FLOATING);
	lv_obj_set_pos(graph_cont, 0, 0);
	lv_obj_set_size(graph_cont, LV_PCT(100), LV_PCT(100));
      }
      lv_obj_move_foreground(graph_cont);
      lv_obj_update_layout(graph_cont);
      fit_graph();
      image_fullsized = !image_fullsized;
    }
  }
}

void BeltsCalibrationPanel::handle_macro_response(json &j) {
  spdlog::trace("belts calbiration macro response: {}", j.dump());
  auto &v = j["/params/0"_json_pointer];
  if (!v.is_null()) {
    std::string resp = v.template get<std::string>();
    std::lock_guard<std::mutex> lock(lv_lock);
    if (resp.rfind("// Command {ps_belts_calibration} finished", 0) == 0) {
      spdlog::trace("belts calbiration finished");
      auto config_root = KUtils::get_root_path("config");
      auto png_path = fmt::format("{}/{}", config_root.length() > 0 ? config_root : "/tmp" , BELTS_PNG);

      png_path = 
	fmt::format("A:{}", KUtils::is_running_local()
		    ? png_path
		    : KUtils::download_file("config", BELTS_PNG, Config::get_instance()->get_thumbnail_path()));

      lv_img_set_src(graph, png_path.c_str());
      fit_graph();
      lv_obj_clear_flag(graph, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(spinner, LV_OBJ_FLAG_HIDDEN);
      lv_obj_move_background(spinner);
    }
  }
}

void BeltsCalibrationPanel::handle_update_slider(lv_event_t *e) {
  const lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_VALUE_CHANGED) {
    double hz = (double)lv_slider_get_value(excite_slider);
    lv_slider_set_value(excite_slider, hz, LV_ANIM_OFF);
    lv_label_set_text(excite_label, fmt::format("{} hz", hz / 10.0 ).c_str());
  }
}
