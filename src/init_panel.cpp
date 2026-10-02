#include "init_panel.h"
#include "utils.h"
#include "state.h"
#include "config.h"
#include "powerui.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cstdio>

InitPanel::InitPanel(MainPanel &mp, BedMeshPanel &bmp, std::mutex& l)
  : cont(lv_obj_create(lv_scr_act()))
  , spinner(NULL)
  , label(NULL)
  , main_panel(mp)
  , bedmesh_panel(bmp)
  , lv_lock(l)
{
  using namespace powerui;

  // PowerUI notice: a card with a green spinner and the message, centered at the top.
  lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(cont, LV_SIZE_CONTENT, px(52));
  lv_obj_align(cont, LV_ALIGN_TOP_MID, 0, px(14));
  lv_obj_set_style_bg_color(cont, lv_color_hex(COLOR_CARD), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(cont, px(12), LV_PART_MAIN);
  lv_obj_set_style_border_width(cont, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(cont, lv_color_hex(COLOR_WHITE), LV_PART_MAIN);
  lv_obj_set_style_border_opa(cont, LV_OPA_20, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(cont, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_hor(cont, px(20), LV_PART_MAIN);
  lv_obj_set_style_pad_ver(cont, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_column(cont, px(14), LV_PART_MAIN);
  lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  Config *conf = Config::get_instance();
  bool waiting = !conf->get_json("/default_printer").is_null();

  if (waiting) {
    spinner = lv_spinner_create(cont, 1000, 60);
    lv_obj_set_size(spinner, px(22), px(22));
    lv_obj_set_style_arc_width(spinner, 3, LV_PART_MAIN);
    lv_obj_set_style_arc_width(spinner, 3, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(spinner, lv_color_hex(COLOR_SECONDARY), LV_PART_MAIN);
    lv_obj_set_style_arc_color(spinner, lv_color_hex(COLOR_ACCENT), LV_PART_INDICATOR);
    lv_obj_clear_flag(spinner, LV_OBJ_FLAG_CLICKABLE);
  }

  label = powerui::label(cont, waiting ? "Waiting for printer to initialize..."
                                       : "Welcome to PowerScreen. Use the Setting Panel to add your printers.",
                         &lv_font_montserrat_16, lv_color_hex(COLOR_FG));
}

InitPanel::~InitPanel() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

void InitPanel::connected(KWebSocketClient &ws) {
  spdlog::debug("init panel connected");
  State *state = State::get_instance();
  state->reset();

  ws.send_jsonrpc("printer.objects.list", [this, &ws](json& d) {
    State *state = State::get_instance();
	state->set_data("printer_objs", d, "/result");

	ws.send_jsonrpc("server.files.roots",
			[](json& j) { State::get_instance()->set_data("roots", j, "/result"); });

	ws.send_jsonrpc("printer.info",
			[](json& j) { State::get_instance()->set_data("printer_info", j, "/result"); });
	
	json h = {
	  { "namespace", "fluidd" },
	  { "key", "console" }
	};
	ws.send_jsonrpc("server.database.get_item", h,
			[](json& j) { State::get_instance()->set_data("console", j, "/result/value"); });

	h = {
	  { "namespace", "powerscreen" }
	};
	ws.send_jsonrpc("server.database.get_item", h,
			[](json& j) { State::get_instance()->set_data("powerscreensettings", j, "/result/value"); });

	// console
	this->main_panel.subscribe();

	// spoolman
	ws.send_jsonrpc("server.info", [this](json &j) {
	  spdlog::debug("server_info {}", j.dump());
	  State::get_instance()->set_data("server_info", j, "/result");
	  
	  auto &components = j["/result/components"_json_pointer];
	  if (!components.is_null()) {
	    const auto &has_spoolman = components.template get<std::vector<std::string>>();
	    if (std::find(has_spoolman.begin(), has_spoolman.end(), "spoolman") != has_spoolman.end()) {
	      this->main_panel.enable_spoolman();
	    }
	  }
	});

	auto display_sensors = state->get_display_sensors();
	this->main_panel.create_sensors(display_sensors);

	auto display_fans = state->get_display_fans();
	this->main_panel.create_fans(display_fans);

	auto display_leds = state->get_display_leds();
	this->main_panel.create_leds(display_leds);

	// subscribe to all objects except gcode_macro
	auto objs = d["/result/objects"_json_pointer];
	if (!objs.is_null()) {
	  json sub_objs;
	  for (auto &obj : objs) {
	    std::string obj_name = obj.template get<std::string>();
	    if (obj_name.rfind("gcode_macro ", 0 ) != 0) {
	      sub_objs[obj_name] = nullptr;
	    }
	  }

	  sub_objs["tmcstatus"] = nullptr;

	  json subs = {{ "objects", sub_objs }};
	  spdlog::debug("subcribing to {}", subs.dump());
	  ws.send_jsonrpc("printer.objects.subscribe", subs,
			  [this](json &data) {
			    State::get_instance()->set_data("printer_state",
							    data, "/result/status");
			    this->main_panel.init(data);
			    this->bedmesh_panel.refresh_views_with_lock(
									data["/result/status/bed_mesh"_json_pointer]);
			    spdlog::debug("done init");
			    std::lock_guard<std::mutex> lock(this->lv_lock);
			    lv_obj_add_flag(this->cont, LV_OBJ_FLAG_HIDDEN);
			    lv_obj_move_background(this->cont);
					
			  });
	}
  });

  ws.send_jsonrpc("machine.device_power.devices", [this](json& j) {
    main_panel.get_tune_panel().set_power_devices(j);
  });
}

void InitPanel::disconnected(KWebSocketClient &ws) {
  spdlog::debug("init panel disconnected");
  std::lock_guard<std::mutex> lock(lv_lock);
  lv_obj_clear_flag(cont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(cont);
}

void InitPanel::set_message(const char *message) {
	lv_label_set_text(label, message);
}
