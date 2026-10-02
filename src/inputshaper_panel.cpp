#include "inputshaper_panel.h"
#include "powerui.h"
#include "state.h"
#include "utils.h"
#include "config.h"
#include "spdlog/spdlog.h"

#include <algorithm>

LV_IMG_DECLARE(ui_icon_play);
LV_IMG_DECLARE(ui_icon_save);
LV_IMG_DECLARE(ui_icon_estop);
LV_IMG_DECLARE(emergency);
LV_IMG_DECLARE(back);

namespace {
// Axis card geometry (design px, overlay coordinates); the graph sits at the top of the card.
constexpr int CARD_W = 350;
constexpr int CARD_H = 326;
constexpr int GRAPH_X = 14;
constexpr int GRAPH_Y = 56;
constexpr int GRAPH_W = 322;
constexpr int GRAPH_H = 150;
}

LV_FONT_DECLARE(dejavusans_mono_14);

#define X_DATA "/tmp/resonances_x_x.csv"
#define X_PNG "resonances_x.png"
#define Y_DATA "/tmp/resonances_y_y.csv"
#define Y_PNG "resonances_y.png"

std::vector<std::string> InputShaperPanel::shapers = {
  "zv",
  "mzv",
  "ei",
  "2hump_ei",
  "3hump_ei"
};

InputShaperPanel::InputShaperPanel(KWebSocketClient &c, std::mutex &l)
  : ws(c)
  , lv_lock(l)
  , cont(lv_obj_create(lv_scr_act()))

    // xgraph
  , xgraph_cont(lv_obj_create(cont))
  , xgraph(lv_img_create(xgraph_cont))
  , xoutput(lv_label_create(cont))
  , xspinner(lv_spinner_create(cont, 1000, 60))

    // ygraph
  , ygraph_cont(lv_obj_create(cont))
  , ygraph(lv_img_create(ygraph_cont))
  , youtput(lv_label_create(cont))
  , yspinner(lv_spinner_create(cont, 1000, 60))

    // x controls    
  , xcontrol(lv_obj_create(cont))
  , xaxis_label(lv_label_create(xcontrol))
  , x_switch(lv_switch_create(xcontrol))
  , xslider_cont(lv_obj_create(xcontrol))
  , xslider(lv_slider_create(xslider_cont))
  , xlabel(lv_label_create(xslider_cont))
  , xshaper_dd(lv_dropdown_create(xcontrol))

    // y controls
  , ycontrol(lv_obj_create(cont))
  , yaxis_label(lv_label_create(ycontrol))
  , y_switch(lv_switch_create(ycontrol))
  , yslider_cont(lv_obj_create(ycontrol))
  , yslider(lv_slider_create(yslider_cont))
  , ylabel(lv_label_create(yslider_cont))
  , yshaper_dd(lv_dropdown_create(ycontrol))
    
  , button_cont(lv_obj_create(cont))
  , switch_cont(lv_obj_create(button_cont))
  , graph_switch_label(lv_label_create(switch_cont))
  , graph_switch(lv_switch_create(switch_cont))
  , calibrate_btn(NULL)
  , save_btn(NULL)
  , stop_btn(NULL)
  , emergency_btn(cont, &emergency, "Stop", &InputShaperPanel::_handle_callback, this,
		  "Do you want to emergency stop?",
		  [&c]() {
		    spdlog::debug("emergency stop pressed");
		    c.send_jsonrpc("printer.emergency_stop");
		  })
  , back_btn(cont, &back, "Back", &InputShaperPanel::_handle_callback, this)
  , ximage_fullsized(false)
  , yimage_fullsized(false)
{
  using namespace powerui;
  lv_obj_move_background(cont);
  style_overlay_root(cont);
  lv_obj_add_flag(emergency_btn.get_container(), LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(back_btn.get_container(), LV_OBJ_FLAG_HIDDEN);

  struct AxisWidgets {
    lv_obj_t *card, *axis_label, *sw, *graph_cont, *graph, *output, *spinner, *slider_cont, *slider, *value, *dd;
    const char *name;
    int x;
  };
  AxisWidgets axes_w[2] = {
    {xcontrol, xaxis_label, x_switch, xgraph_cont, xgraph, xoutput, xspinner, xslider_cont, xslider, xlabel, xshaper_dd, "X axis", 12},
    {ycontrol, yaxis_label, y_switch, ygraph_cont, ygraph, youtput, yspinner, yslider_cont, yslider, ylabel, yshaper_dd, "Y axis", 374},
  };

  for (auto &a : axes_w) {
    // The axis control container becomes the card; its widgets are laid out by hand.
    lv_obj_remove_style_all(a.card);
    lv_obj_clear_flag(a.card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(a.card, px(a.x), px(12));
    lv_obj_set_size(a.card, px(CARD_W), px(CARD_H));
    lv_obj_set_style_bg_color(a.card, lv_color_hex(COLOR_CARD), 0);
    lv_obj_set_style_bg_opa(a.card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(a.card, px(14), 0);
    lv_obj_set_style_border_width(a.card, 1, 0);
    lv_obj_set_style_border_color(a.card, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_border_opa(a.card, LV_OPA_10, 0);

    lv_label_set_text(a.axis_label, a.name);
    lv_obj_set_width(a.axis_label, LV_SIZE_CONTENT);
    lv_obj_set_style_text_font(a.axis_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(a.axis_label, lv_color_hex(COLOR_FG), 0);
    lv_obj_set_pos(a.axis_label, px(18), px(18));

    style_switch(a.sw);
    lv_obj_align(a.sw, LV_ALIGN_TOP_RIGHT, -px(18), px(16));
    lv_obj_add_state(a.sw, LV_STATE_CHECKED);

    // Graph / result area.
    lv_obj_set_parent(a.graph_cont, a.card);
    lv_obj_set_pos(a.graph_cont, px(GRAPH_X), px(GRAPH_Y));
    lv_obj_set_size(a.graph_cont, px(GRAPH_W), px(GRAPH_H));
    lv_obj_set_style_pad_all(a.graph_cont, 0, 0);
    lv_obj_set_style_radius(a.graph_cont, px(10), 0);
    lv_obj_set_style_border_width(a.graph_cont, 0, 0);
    lv_obj_set_style_bg_color(a.graph_cont, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(a.graph_cont, LV_OPA_COVER, 0);
    lv_obj_set_style_clip_corner(a.graph_cont, true, 0);
    lv_obj_add_flag(a.graph_cont, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(a.graph_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(a.graph_cont, &InputShaperPanel::_handle_image_clicked, LV_EVENT_CLICKED, this);
    lv_obj_clear_flag(a.graph, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(a.graph);
    lv_obj_add_flag(a.graph_cont, LV_OBJ_FLAG_HIDDEN);

    lv_obj_set_parent(a.output, a.card);
    lv_obj_set_pos(a.output, px(GRAPH_X + 8), px(GRAPH_Y + 6));
    lv_obj_set_size(a.output, px(GRAPH_W - 16), px(GRAPH_H - 12));
    lv_label_set_text(a.output, "");
    lv_obj_set_style_text_font(a.output, &dejavusans_mono_14, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(a.output, lv_color_hex(COLOR_FG), 0);

    lv_obj_t *hint = label(a.card, "Run Calibrate to measure this axis", &lv_font_montserrat_12, lv_color_hex(COLOR_MUTED));
    lv_obj_set_pos(hint, px(GRAPH_X + 8), px(GRAPH_Y + GRAPH_H / 2 - 8));
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(hint, px(GRAPH_W - 16));
    lv_obj_move_background(hint);

    lv_obj_set_parent(a.spinner, a.card);
    lv_obj_add_flag(a.spinner, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_size(a.spinner, px(60), px(60));
    lv_obj_set_pos(a.spinner, px(GRAPH_X + GRAPH_W / 2 - 30), px(GRAPH_Y + GRAPH_H / 2 - 30));

    // Shaper type, frequency and slider.
    lv_obj_t *shaper_title = label(a.card, "Shaper", &lv_font_montserrat_14, lv_color_hex(COLOR_MUTED));
    lv_obj_set_pos(shaper_title, px(18), px(224));
    lv_obj_set_size(a.dd, px(150), px(40));
    lv_obj_set_pos(a.dd, px(CARD_W - 18 - 150), px(214));
    style_select(a.dd);

    lv_obj_t *freq_title = label(a.card, "Frequency", &lv_font_montserrat_14, lv_color_hex(COLOR_MUTED));
    lv_obj_set_pos(freq_title, px(18), px(274));

    lv_obj_set_parent(a.value, a.card);
    lv_label_set_text(a.value, "0 Hz");
    lv_obj_set_style_text_font(a.value, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(a.value, lv_color_hex(COLOR_FG), 0);
    lv_obj_align(a.value, LV_ALIGN_TOP_RIGHT, -px(18), px(268));

    lv_obj_set_parent(a.slider, a.card);
    lv_obj_set_size(a.slider, px(CARD_W - 36), px(14));
    lv_obj_set_pos(a.slider, px(18), px(302));
    lv_slider_set_range(a.slider, 0, 1400);
    style_slider(a.slider);
    lv_obj_add_event_cb(a.slider, &InputShaperPanel::_handle_update_slider, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_add_flag(a.slider_cont, LV_OBJ_FLAG_HIDDEN);

    lv_obj_set_size(a.card, px(CARD_W), px(CARD_H));
  }

  lv_dropdown_set_options(xshaper_dd, fmt::format("{}", fmt::join(shapers, "\n")).c_str());
  lv_dropdown_set_options(yshaper_dd, fmt::format("{}", fmt::join(shapers, "\n")).c_str());
  style_select(xshaper_dd);
  style_select(yshaper_dd);

  // Bottom bar: graphs switch, hints and the actions.
  lv_obj_remove_style_all(button_cont);
  lv_obj_clear_flag(button_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_pos(button_cont, px(12), px(350));
  lv_obj_set_size(button_cont, px(712), px(78));
  lv_obj_set_style_bg_color(button_cont, lv_color_hex(COLOR_CARD), 0);
  lv_obj_set_style_bg_opa(button_cont, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(button_cont, px(14), 0);
  lv_obj_set_style_border_width(button_cont, 1, 0);
  lv_obj_set_style_border_color(button_cont, lv_color_hex(COLOR_WHITE), 0);
  lv_obj_set_style_border_opa(button_cont, LV_OPA_10, 0);

  lv_obj_remove_style_all(switch_cont);
  lv_obj_clear_flag(switch_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(switch_cont, px(190), px(54));
  lv_obj_set_pos(switch_cont, px(20), px(12));
  lv_obj_set_flex_flow(switch_cont, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(switch_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(switch_cont, px(12), 0);
  lv_obj_set_style_pad_row(switch_cont, px(4), 0);
  lv_label_set_text(graph_switch_label, "Graphs");
  lv_obj_set_width(graph_switch_label, LV_SIZE_CONTENT);
  lv_obj_set_style_text_font(graph_switch_label, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(graph_switch_label, lv_color_hex(COLOR_FG), 0);
  style_switch(graph_switch);
  lv_obj_clear_state(graph_switch, LV_STATE_CHECKED);
  lv_obj_t *note = label(switch_cont, "Needs an accelerometer. Save restarts Klipper", &lv_font_montserrat_10, lv_color_hex(COLOR_MUTED));
  lv_label_set_long_mode(note, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(note, px(190));

  calibrate_btn = action_button(button_cont, &ui_icon_play, "Calibrate", ActionKind::Primary, 230, 12, 190, 54,
				&InputShaperPanel::_handle_callback, this);
  save_btn = action_button(button_cont, &ui_icon_save, "Save", ActionKind::Outline, 430, 12, 140, 54,
			   &InputShaperPanel::_handle_callback, this);
  stop_btn = action_button(button_cont, &ui_icon_estop, "Stop", ActionKind::Destructive, 580, 12, 120, 54,
			   &InputShaperPanel::_handle_callback, this);

  // TODO: show only register when issuing macros inputshaper cares about, then unregister after.
  // ws.register_gcode_resp([this](json& d) { this->handle_macro_response(d); });
  ws.register_method_callback("notify_gcode_response",
			      "InputShaperPanel",
			      [this](json& d) { this->handle_macro_response(d); });
}

InputShaperPanel::~InputShaperPanel() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

void InputShaperPanel::foreground() {
  auto inputshaper = State::get_instance()
    ->get_data("/printer_state/configfile/config/input_shaper"_json_pointer);
  spdlog::trace("input shapper {}", inputshaper.dump());

  if (!inputshaper.is_null()) {
    auto v = inputshaper["/shaper_freq_x"_json_pointer];
    if (!v.is_null()) {
      double hz = std::stod(v.template get<std::string>());
      lv_slider_set_value(xslider, hz * 10, LV_ANIM_OFF);
      lv_label_set_text(xlabel, fmt::format("{} Hz", hz).c_str());
    }

    v = inputshaper["/shaper_type_x"_json_pointer];
    if (!v.is_null()) {
      auto shaper = v.template get<std::string>();
      auto idx = find_shaper_index(shapers, shaper);
      lv_dropdown_set_selected(xshaper_dd, idx);
    }    

    v = inputshaper["/shaper_freq_y"_json_pointer];
    if (!v.is_null()) {
      double hz = std::stod(v.template get<std::string>());
      lv_slider_set_value(yslider, hz * 10, LV_ANIM_OFF);
      lv_label_set_text(ylabel, fmt::format("{} Hz", hz).c_str()); 
    }

    v = inputshaper["/shaper_type_y"_json_pointer];
    if (!v.is_null()) {
      auto shaper = v.template get<std::string>();
      auto idx = find_shaper_index(shapers, shaper);
      lv_dropdown_set_selected(yshaper_dd, idx);
    }
  }
  
  lv_obj_move_foreground(cont);
  lv_obj_add_flag(back_btn.get_container(), LV_OBJ_FLAG_HIDDEN);  // Back lives in the title bar
  powerui::overlay_open("Input Shaper", [this]() { lv_obj_move_background(cont); });
}


void InputShaperPanel::handle_callback(lv_event_t *event) {
  lv_obj_t *btn = lv_event_get_current_target(event);
  if (btn == calibrate_btn) {
    bool x_requested = lv_obj_has_state(x_switch, LV_STATE_CHECKED);
    bool y_requested = lv_obj_has_state(y_switch, LV_STATE_CHECKED);

    if ((x_requested || y_requested) && !KUtils::is_homed()) {
      ws.gcode_script("G28");
    }

    if (x_requested) {
      // ws.gcode_script(fmt::format("TEST_RESONANCES AXIS=X NAME=x FREQ_START={} FREQ_END={}\nM400", 5, 10));
      ws.gcode_script(fmt::format("TEST_RESONANCES AXIS=X NAME=x\nM400"));

      // free src      
      lv_img_set_src(xgraph, NULL);
      // hack to color in empty space.
      ((lv_img_t*)xgraph)->src_type = LV_IMG_SRC_SYMBOL;
      
      lv_label_set_text(xoutput, "");
      lv_obj_add_flag(xgraph_cont, LV_OBJ_FLAG_HIDDEN);      
      lv_obj_clear_flag(xspinner, LV_OBJ_FLAG_HIDDEN);
      lv_obj_move_foreground(xspinner);
    }

    if (y_requested) {
      // ws.gcode_script(fmt::format("TEST_RESONANCES AXIS=Y NAME=y FREQ_START={} FREQ_END={}\nM400", 5, 10));
      ws.gcode_script(fmt::format("TEST_RESONANCES AXIS=Y NAME=y\nM400"));

      // free src
      lv_img_set_src(ygraph, NULL);
      // hack to color in empty space.
      ((lv_img_t*)ygraph)->src_type = LV_IMG_SRC_SYMBOL;
      
      lv_label_set_text(youtput, ""); 
      lv_obj_add_flag(ygraph_cont, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(yspinner, LV_OBJ_FLAG_HIDDEN);
      lv_obj_move_foreground(yspinner);
    }
    // ws.gcode_script(fmt::format("TEST_RESONANCES AXIS=X NAME=x FREQ_START={} FREQ_END={}\nM400\nTEST_RESONANCES AXIS=Y NAME=y FREQ_START={} FREQ_END={}\nM400", 5, 10, 5, 10));
    // ws.gcode_script(fmt::format("TEST_RESONANCES AXIS=X NAME=x FREQ_START={} FREQ_END={}\nM400", 5, 10));
    // ws.gcode_script(fmt::format("TEST_RESONANCES AXIS=X NAME=x\nM400\nTEST_RESONANCES AXIS=Y NAME=y\nM400"));

  } else if (btn == save_btn) {
    double xhz = (double)lv_slider_get_value(xslider) / 10.0;
    double yhz = (double)lv_slider_get_value(yslider) / 10.0;

    char xbuf[10];
    lv_dropdown_get_selected_str(xshaper_dd, xbuf, sizeof(xbuf));

    char ybuf[10];
    lv_dropdown_get_selected_str(yshaper_dd, ybuf, sizeof(ybuf));
    
    ws.gcode_script(fmt::format("PS_SAVE_INPUT_SHAPER SHAPER_FREQ_X={} SHAPER_TYPE_X={} SHAPER_FREQ_Y={} SHAPER_TYPE_Y={}\nSAVE_CONFIG",
				xhz, xbuf, yhz, ybuf));

  } else if (btn == back_btn.get_container()) {
    lv_obj_move_background(cont);
  } else if (btn == stop_btn || btn == emergency_btn.get_container()) {
    emergency_stop();
  }
}

void InputShaperPanel::handle_macro_response(json &j) {
  spdlog::trace("inputshapping macro response: {}", j.dump());
  auto &v = j["/params/0"_json_pointer];
  if (!v.is_null()) {
    std::string resp = v.template get<std::string>();
    std::lock_guard<std::mutex> lock(lv_lock);
    bool graph_requested = lv_obj_has_state(graph_switch, LV_STATE_CHECKED);
    if (resp.rfind("// {\"shapers\":", 0) == 0) {
      auto res = json::parse(resp.substr(3));
      auto &axis_log = res["/logfile"_json_pointer];
      auto &axis_png = res["/png"_json_pointer];
      
      if (!axis_log.is_null()) {
	std::string fn = axis_log.template get<std::string>();
	if (X_DATA == fn) {
	  if (graph_requested && !axis_png.is_null()) {
	    spdlog::trace("found generated x freq png");
	    auto config_root = KUtils::get_root_path("config");
	    auto png_path = fmt::format("{}/{}", config_root.length() > 0 ? config_root : "/tmp" , X_PNG);

	    png_path = 
	      fmt::format("A:{}", KUtils::is_running_local()
			  ? png_path
			  : KUtils::download_file("config", X_PNG, Config::get_instance()->get_thumbnail_path()));

	    spdlog::trace("x freq png path {}", png_path);
	      
	    lv_label_set_text(xoutput, "");
	    lv_img_set_src(xgraph, png_path.c_str());
	    fit_graph(xgraph_cont, xgraph);
	    lv_obj_clear_flag(xgraph_cont, LV_OBJ_FLAG_HIDDEN);
	    set_shaper_detail(res, NULL, xslider, xlabel, xshaper_dd);
	    lv_obj_move_foreground(xgraph_cont);
	  } else {
	    set_shaper_detail(res, xoutput, xslider, xlabel, xshaper_dd);
	    lv_obj_move_foreground(xoutput);
	  }
	  
	  lv_obj_add_flag(xspinner, LV_OBJ_FLAG_HIDDEN);
	  lv_obj_move_background(xspinner);

	} else if (Y_DATA == fn) {
	  if (graph_requested && !axis_png.is_null()) {
	    spdlog::trace("found generated y freq png");
	    auto config_root = KUtils::get_root_path("config");
	    auto png_path = fmt::format("{}/{}", config_root.length() > 0 ? config_root : "/tmp" , Y_PNG);

	    png_path = 
	      fmt::format("A:{}", KUtils::is_running_local()
			  ? png_path
			  : KUtils::download_file("config", Y_PNG, Config::get_instance()->get_thumbnail_path()));

	    spdlog::trace("y freq png path {}", png_path);

	    lv_label_set_text(youtput, "");
	    lv_img_set_src(ygraph, png_path.c_str());
	    fit_graph(ygraph_cont, ygraph);
	    lv_obj_clear_flag(ygraph_cont, LV_OBJ_FLAG_HIDDEN);
	    set_shaper_detail(res, NULL, yslider, ylabel, yshaper_dd);
	    lv_obj_move_foreground(ygraph_cont);
	  } else {
	    set_shaper_detail(res, youtput, yslider, ylabel, yshaper_dd);
	    lv_obj_move_foreground(youtput);
	  }

	  lv_obj_add_flag(yspinner, LV_OBJ_FLAG_HIDDEN);
	  lv_obj_move_background(yspinner);
	}
      }

    } else if ("// Resonances data written to " Y_DATA " file" == resp) {
      auto config_root = KUtils::get_root_path("config");
      auto screen_width = (double)powerui::overlay_width_px() / 100.0;
      auto screen_height = (double)powerui::overlay_height_px() / 100.0;
      auto png_path = fmt::format("{}/{}", config_root.length() > 0 ? config_root : "/tmp" , Y_PNG);
      std::string arg = graph_requested
	? fmt::format("{} -o {} -w {} -l {}", Y_DATA, png_path, screen_width, screen_height)
	: Y_DATA;

      ws.gcode_script(fmt::format("RUN_SHELL_COMMAND CMD=ps_input_shaper PARAMS={:?}", arg));

    } else if ("// Resonances data written to " X_DATA " file" == resp) {
      auto config_root = KUtils::get_root_path("config");
      auto screen_width = (double)powerui::overlay_width_px() / 100.0;
      auto screen_height = (double)powerui::overlay_height_px() / 100.0;
      auto png_path = fmt::format("{}/{}", config_root.length() > 0 ? config_root : "/tmp" , X_PNG);
      std::string arg = graph_requested
	? fmt::format("{} -o {} -w {} -l {}", X_DATA, png_path, screen_width, screen_height)
	: X_DATA;

      ws.gcode_script(fmt::format("RUN_SHELL_COMMAND CMD=ps_input_shaper PARAMS={:?}", arg));
    }
  }
}

void InputShaperPanel::emergency_stop() {
  Config *conf = Config::get_instance();
  auto estop = conf->get_json("/prompt_emergency_stop");
  if (!estop.is_null() && estop.template get<bool>()) {
    emergency_btn.handle_prompt();
  } else {
    ws.send_jsonrpc("printer.emergency_stop");
  }
}

// Scale the graph PNG to fit (contain) inside its box.
void InputShaperPanel::fit_graph(lv_obj_t *graph_cont, lv_obj_t *graph) {
  lv_img_header_t header;
  const void *src = lv_img_get_src(graph);
  if (src == NULL || lv_img_decoder_get_info(src, &header) != LV_RES_OK || header.w == 0 || header.h == 0) {
    return;
  }
  const double fit = std::min((double)lv_obj_get_width(graph_cont) / header.w, (double)lv_obj_get_height(graph_cont) / header.h);
  lv_img_set_zoom(graph, static_cast<uint16_t>(256 * fit));
  lv_obj_center(graph);
}

// Enlarge a graph over the whole panel (and back into its axis card).
void InputShaperPanel::toggle_graph(lv_obj_t *graph_cont, lv_obj_t *graph, lv_obj_t *axis_card, bool &fullsized) {
  if (fullsized) {
    lv_obj_set_parent(graph_cont, axis_card);
    lv_obj_clear_flag(graph_cont, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_pos(graph_cont, powerui::px(GRAPH_X), powerui::px(GRAPH_Y));
    lv_obj_set_size(graph_cont, powerui::px(GRAPH_W), powerui::px(GRAPH_H));
  } else {
    lv_obj_set_parent(graph_cont, cont);
    lv_obj_add_flag(graph_cont, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_pos(graph_cont, 0, 0);
    lv_obj_set_size(graph_cont, LV_PCT(100), LV_PCT(100));
  }
  lv_obj_move_foreground(graph_cont);
  lv_obj_update_layout(graph_cont);
  fit_graph(graph_cont, graph);
  fullsized = !fullsized;
}

void InputShaperPanel::handle_image_clicked(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    lv_obj_t *clicked = lv_event_get_target(e);
    if (clicked == xgraph_cont) {
      toggle_graph(xgraph_cont, xgraph, xcontrol, ximage_fullsized);
    } else if (clicked == ygraph_cont) {
      toggle_graph(ygraph_cont, ygraph, ycontrol, yimage_fullsized);
    }
  }
}

void InputShaperPanel::handle_update_slider(lv_event_t *e) {
  const lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_VALUE_CHANGED) {
    lv_obj_t * obj = lv_event_get_target(e);
    double hz = (double)lv_slider_get_value(obj);
    lv_slider_set_value(obj, hz, LV_ANIM_OFF);
    if (obj == xslider) {
      lv_label_set_text(xlabel, fmt::format("{} hz", hz / 10.0 ).c_str());
    } else if (obj == yslider) {
      lv_label_set_text(ylabel, fmt::format("{} hz", hz / 10.0).c_str());      
    }
  }
}

uint32_t InputShaperPanel::find_shaper_index(const std::vector<std::string> &s,
					     const std::string &shaper) {
  return std::distance(s.cbegin(), std::find(s.cbegin(), s.cend(), shaper));
}

void InputShaperPanel::set_shaper_detail(json &res,
					 lv_obj_t *label,
					 lv_obj_t *slider,
					 lv_obj_t *slider_label,
					 lv_obj_t *dd) {
  auto &shapers_resp = res["/shapers"_json_pointer];
  if (!shapers_resp.is_null()) {
    std::vector<std::string> shaper_details;
    auto &best_shaper = res["/best"_json_pointer];
    if (!best_shaper.is_null()) {
      auto bs_name = best_shaper.template get<std::string>();
      double f = shapers_resp[bs_name]["freq"]
	.template get<double>();
      shaper_details.push_back(fmt::format("Best shaper is {} @ {:.2f} Hz\n", bs_name, f));

      uint32_t idx = find_shaper_index(shapers, bs_name);
      lv_dropdown_set_selected(dd, idx);

      lv_slider_set_value(slider, f * 10, LV_ANIM_OFF);
      lv_label_set_text(slider_label, fmt::format("{:.1f} Hz", f).c_str());
    }

    if (label != NULL) {
      shaper_details.push_back(fmt::format("{:^8}\t{:^4}\t{:^5}\t{:^5}\t{:^5}",
					   "Shaper", "Hz", "Vibr", "Smt", "MaxAcl"));
      for (auto &el : shapers_resp.items()) {
	shaper_details.push_back(
				 fmt::format("{:<8}\t{:.1f}\t{:.1f}%\t{:.3f}\t{:>}",
					     el.key(),
					     el.value()["freq"].template get<double>(),
					     el.value()["vib"].template get<double>(),
					     el.value()["smooth"].template get<double>(),
					     el.value()["max_acel"].template get<double>()));
      }

      lv_label_set_text(label, fmt::format("{}", fmt::join(shaper_details, "\n")).c_str());
    }
  }
}
