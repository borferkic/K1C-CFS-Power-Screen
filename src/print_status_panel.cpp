#include "print_status_panel.h"
#include "finetune_panel.h"
#include "state.h"
#include "utils.h"
#include "spdlog/spdlog.h"


LV_IMG_DECLARE(extruder);
LV_IMG_DECLARE(speed_up_img);
LV_IMG_DECLARE(extrude);
LV_IMG_DECLARE(clock_img);
LV_IMG_DECLARE(hourglass);
LV_IMG_DECLARE(bed);
LV_IMG_DECLARE(home_z);
LV_IMG_DECLARE(fan);
LV_IMG_DECLARE(layers_img);

LV_IMG_DECLARE(fine_tune_img);
LV_IMG_DECLARE(pause_img);
LV_IMG_DECLARE(resume);
LV_IMG_DECLARE(cancel);
LV_IMG_DECLARE(emergency);
LV_IMG_DECLARE(back);

namespace {
lv_color_t system_background_color() {
  return lv_palette_darken(LV_PALETTE_GREY, 4);
}

void style_detail_item(lv_obj_t *item) {
  lv_obj_set_width(item, LV_PCT(48));
  lv_obj_set_height(item, LV_PCT(18));
}
}

double pi() { return std::atan(1)*4; }

PrintStatusPanel::PrintStatusPanel(KWebSocketClient &websocket_client,
				   std::mutex &lock,
				   lv_obj_t *mini_parent)
  : NotifyConsumer(lock)
  , ws(websocket_client)
  , finetune_panel(websocket_client, lock)
  , mini_print_status(mini_parent, &PrintStatusPanel::_handle_callback, this)
  , status_cont(lv_obj_create(lv_scr_act()))
  , title_bar(lv_obj_create(status_cont))
  , title_label(lv_label_create(title_bar))
  , time_label(lv_label_create(title_bar))
  , clock_timer(NULL)
  , buttons_cont(lv_obj_create(status_cont))
  , finetune_btn(buttons_cont, &fine_tune_img, "Fine Tune", &PrintStatusPanel::_handle_callback, this)
  , pause_btn(buttons_cont, &pause_img, "Pause", &PrintStatusPanel::_handle_callback, this)
  , resume_btn(buttons_cont, &resume, "Resume", &PrintStatusPanel::_handle_callback, this)
  , cancel_btn(buttons_cont, &cancel, "Cancel", &PrintStatusPanel::_handle_callback, this,
	       "Do you want to cancel the print?",
	       [&websocket_client]() {
		 spdlog::debug("cancel print prompt");
		 websocket_client.send_jsonrpc("printer.print.cancel");
	       })
  , emergency_btn(buttons_cont, &emergency, "Stop", &PrintStatusPanel::_handle_callback, this,
		  "Do you want to emergency stop?",
		  [&websocket_client]() {
		    spdlog::debug("emergency stop pressed");
		    websocket_client.send_jsonrpc("printer.emergency_stop");
		  })
  , back_btn(buttons_cont, &back, "Back", &PrintStatusPanel::_handle_callback, this)
  , thumbnail_cont(lv_obj_create(status_cont))
  , file_label(lv_label_create(thumbnail_cont))
  , status_label(lv_label_create(thumbnail_cont))
  , thumbnail(lv_img_create(thumbnail_cont))
  , pbar_cont(lv_obj_create(status_cont))
  , progress_bar(lv_bar_create(pbar_cont))
  , progress_label(lv_label_create(pbar_cont))
  , detail_cont(lv_obj_create(status_cont))
  , extruder_temp(detail_cont, &extruder, 100, "20")
  , bed_temp(detail_cont, &bed, 100, "21")
  , print_speed(detail_cont, &speed_up_img, 100, "0 mm/s")
  , z_offset(detail_cont, &home_z, 100, "0.0 mm")
  , flow_rate(detail_cont, &extrude, 100, "0.0 mm3/s")
  , layers(detail_cont, &layers_img, 100, "...")
  , fan0(detail_cont, &fan, 100, "0%")
  , elapsed(detail_cont, &clock_img, 100, "0s")
  , time_left(detail_cont, &hourglass, 100, "...")
  , estimated_time_s(0)
  , filament_diameter(1.75) // XXX: check config
  , extruder_target(-1)
  , heater_bed_target(-1)
{
  lv_obj_move_background(status_cont);
  lv_obj_clear_flag(status_cont, LV_OBJ_FLAG_SCROLLABLE);  
  lv_obj_set_size(status_cont, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_pad_all(status_cont, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_color(status_cont, system_background_color(), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(status_cont, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(status_cont, 0, LV_PART_MAIN);

  // Match the Home title bar while keeping this overlay self-contained.
  lv_obj_add_flag(title_bar, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(title_bar, LV_PCT(100), 32);
  lv_obj_set_pos(title_bar, 0, 0);
  lv_obj_set_style_pad_all(title_bar, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_color(title_bar, lv_color_hex(0x555555), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(title_bar, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(title_bar, 0, LV_PART_MAIN);

  lv_label_set_text(title_label, "PRINT STATUS");
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
  clock_timer = lv_timer_create(&PrintStatusPanel::_update_clock_cb, 1000, this);

  static lv_coord_t grid_main_row_dsc_detail[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
    LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
  static lv_coord_t grid_main_col_dsc_detail[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
  lv_obj_set_grid_dsc_array(detail_cont, grid_main_col_dsc_detail, grid_main_row_dsc_detail);

  lv_obj_clear_flag(detail_cont, LV_OBJ_FLAG_SCROLLABLE);  
  lv_obj_set_size(detail_cont, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(detail_cont, system_background_color(), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(detail_cont, LV_OPA_COVER, LV_PART_MAIN);
  style_detail_item(extruder_temp.get_container());
  style_detail_item(bed_temp.get_container());
  style_detail_item(print_speed.get_container());
  style_detail_item(z_offset.get_container());
  style_detail_item(flow_rate.get_container());
  style_detail_item(layers.get_container());
  style_detail_item(fan0.get_container());
  style_detail_item(elapsed.get_container());
  style_detail_item(time_left.get_container());

  //detail containter row 1
  lv_obj_set_grid_cell(extruder_temp.get_container(), LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_START, 0, 1);
  lv_obj_set_grid_cell(bed_temp.get_container(), LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_START, 0, 1);  

  //detail containter row 2
  lv_obj_set_grid_cell(print_speed.get_container(), LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_START, 1, 1);
  lv_obj_set_grid_cell(z_offset.get_container(), LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_START, 1, 1);  

  //detail containter row 3
  lv_obj_set_grid_cell(flow_rate.get_container(), LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_START, 2, 1);
  lv_obj_set_grid_cell(layers.get_container(), LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_START, 2, 1);

  //detail containter row 4
  lv_obj_set_grid_cell(elapsed.get_container(), LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_START, 3, 1);
  lv_obj_set_grid_cell(fan0.get_container(), LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_START, 3, 1);

  //detail containter row 5
  lv_obj_set_grid_cell(time_left.get_container(), LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_START, 4, 1);
  // lv_obj_set_grid_cell(fan2.get_container(), LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_START, 4, 1);  
  
  auto screen_width = lv_disp_get_physical_hor_res(NULL);
  auto preview_size = static_cast<lv_coord_t>(0.28 * (double)screen_width);
  auto hscale = (double)lv_disp_get_physical_ver_res(NULL) / 480.0;
  auto progress_height = static_cast<lv_coord_t>(24 * hscale);

  static lv_coord_t grid_main_row_dsc[] = {
    32, LV_GRID_FR(1), 28, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST
  };
  static lv_coord_t grid_main_col_dsc[] = {
    LV_GRID_FR(5), LV_GRID_FR(7), LV_GRID_TEMPLATE_LAST
  };

  lv_obj_set_grid_dsc_array(status_cont, grid_main_col_dsc, grid_main_row_dsc);

  lv_obj_set_size(buttons_cont, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_clear_flag(buttons_cont, LV_OBJ_FLAG_SCROLLABLE);  
  lv_obj_set_style_pad_all(buttons_cont, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_top(buttons_cont, 4, LV_PART_MAIN);
  lv_obj_set_style_pad_bottom(buttons_cont, 4, LV_PART_MAIN);
  lv_obj_set_flex_flow(buttons_cont, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(buttons_cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_set_width(file_label, LV_PCT(100));
  lv_obj_set_height(file_label, LV_SIZE_CONTENT);
  lv_obj_add_flag(file_label, LV_OBJ_FLAG_HIDDEN);
  lv_label_set_long_mode(file_label, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_align(file_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_style_text_color(file_label, lv_color_white(), LV_PART_MAIN);
  lv_obj_set_style_text_font(file_label, &lv_font_montserrat_14, LV_PART_MAIN);

  lv_obj_set_width(status_label, LV_PCT(100));
  lv_obj_set_height(status_label, LV_SIZE_CONTENT);
  lv_obj_set_style_text_align(status_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_style_text_color(status_label, lv_color_hex(0x4CAF50), LV_PART_MAIN);
  lv_obj_set_style_text_font(status_label, &lv_font_montserrat_20, LV_PART_MAIN);

  lv_obj_set_style_border_width(thumbnail, 2, LV_PART_MAIN);
  lv_obj_set_style_border_color(thumbnail, lv_color_hex(0x4CAF50), LV_PART_MAIN);
  lv_obj_set_style_border_opa(thumbnail, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(thumbnail, 12, LV_PART_MAIN);
  lv_obj_set_style_clip_corner(thumbnail, true, LV_PART_MAIN);

  lv_obj_set_style_pad_all(pbar_cont, 0, 0);
  lv_obj_clear_flag(pbar_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(pbar_cont, system_background_color(), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(pbar_cont, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(pbar_cont, 0, LV_PART_MAIN);
  // lv_obj_set_style_border_width(pbar_cont, 2, 0);
  // lv_obj_set_style_border_width(thumbnail_cont, 2, 0);

  lv_obj_set_size(pbar_cont, preview_size, progress_height);
  lv_obj_set_size(progress_bar, preview_size, 20 * hscale);
  lv_bar_set_value(progress_bar, 0, LV_ANIM_OFF);
  lv_obj_center(progress_bar);

  lv_label_set_text(progress_label, "0%");
  lv_obj_set_style_text_font(progress_label, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_center(progress_label);

  lv_obj_set_flex_flow(thumbnail_cont, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(thumbnail_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_size(thumbnail_cont, preview_size, preview_size);
  lv_obj_clear_flag(thumbnail_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(thumbnail_cont, system_background_color(), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(thumbnail_cont, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(thumbnail_cont, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(thumbnail_cont, 0, 0);
  lv_obj_set_style_pad_row(thumbnail_cont, 0, 0);

  // row 1
  lv_obj_set_grid_cell(thumbnail_cont, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_START, 1, 1);
  lv_obj_set_grid_cell(detail_cont, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 1, 1);

  // row 2
  lv_obj_set_grid_cell(pbar_cont, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 2, 1);

  // row 3
  lv_obj_set_grid_cell(buttons_cont, LV_GRID_ALIGN_STRETCH, 0, 2, LV_GRID_ALIGN_CENTER, 3, 1);
  
  ws.register_notify_update(this);
}

PrintStatusPanel::~PrintStatusPanel() {
  if (clock_timer != NULL) {
    lv_timer_del(clock_timer);
    clock_timer = NULL;
  }

  if (status_cont != NULL) {
    lv_obj_del(status_cont);
    status_cont = NULL;
  }

  ws.unregister_notify_update(this);
}

void PrintStatusPanel::foreground() {
  // populate();
  lv_obj_move_foreground(status_cont);
}

void PrintStatusPanel::background() {
  lv_obj_move_background(status_cont);
}

void PrintStatusPanel::reset() {
  lv_bar_set_value(progress_bar, 0, LV_ANIM_OFF);
  lv_label_set_text(progress_label, "0%");
  print_speed.update_label("0 mm/s");
  flow_rate.update_label("0.0 mm3/s");
  elapsed.update_label("0s");
  time_left.update_label("...");
  estimated_time_s = 0;
  lv_label_set_text(file_label, "No active print");
  update_status_label("ready");

  auto v = State::get_instance()
    ->get_data("/printer_state/configfile/config/extruder/filament_diameter"_json_pointer);
  filament_diameter = v.is_null() ? 1.750 : std::stod(v.template get<std::string>());
  extruder_target = -1;
  heater_bed_target = -1;

  // free src
  lv_img_set_src(thumbnail, NULL);
  // hack to color in empty space.
  ((lv_img_t*)thumbnail)->src_type = LV_IMG_SRC_SYMBOL;

  mini_print_status.reset();
}

void PrintStatusPanel::init(json &fans) {
  fan_speeds.clear();
  std::vector<std::string> values;
  for (auto &f : fans.items()) {
    std::string fan_name = f.key();

    auto fan_value = State::get_instance()
      ->get_data(json::json_pointer(fmt::format("/printer_state/{}/value", fan_name)));    
    if (!fan_value.is_null()) {
      int v = static_cast<int>(fan_value.template get<double>() * 100);
      fan_speeds.insert({fan_name, v});
      values.push_back(fmt::format("{}%", v));
    }

    fan_value = State::get_instance()
      ->get_data(json::json_pointer(fmt::format("/printer_state/{}/speed", fan_name)));
    if (!fan_value.is_null()) {
      int v = static_cast<int>(fan_value.template get<double>() * 100);
      fan_speeds.insert({fan_name, v});
      values.push_back(fmt::format("{}%", v));
    }
  }

  fan0.update_label(fmt::format("{}", fmt::join(values, ", ")).c_str());

  reset();
  populate();
  json &pstat_state = State::get_instance()
    ->get_data("/printer_state/print_stats/state"_json_pointer);
  if (!pstat_state.is_null()) {
    auto pstatus = pstat_state.template get<std::string>();
    update_status_label(pstatus);
    if (pstatus != "printing" && pstatus != "paused") {
      mini_print_status.hide();
    }
    mini_print_status.update_status(pstatus);
  } else {
    update_status_label("ready");
    mini_print_status.show();
  }
  
}

void PrintStatusPanel::populate() {
  State* s = State::get_instance();
  json& printfile = s->get_data("/printer_state/print_stats/filename"_json_pointer);
  if (!printfile.is_null()) {
    const std::string fname = printfile.template get<std::string>();
    if (fname.length() > 0) {
      const size_t separator = fname.find_last_of("/\\");
      const std::string display_name = separator == std::string::npos
        ? fname
        : fname.substr(separator + 1);
      lv_label_set_text(file_label, display_name.c_str());

      json fname_input = {{"filename", fname }};
      ws.send_jsonrpc("server.files.metadata", fname_input,
		      [fname, this](json &d) { this->handle_metadata(fname, d); });

      mini_print_status.show();
    }
  }

  auto& pstate = s->get_data("/printer_state/print_stats/state"_json_pointer);
  if (!pstate.is_null() && pstate.template get<std::string>() == "paused") {
    lv_obj_clear_flag(resume_btn.get_container(), LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(pause_btn.get_container(), LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(resume_btn.get_container(), LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(pause_btn.get_container(), LV_OBJ_FLAG_HIDDEN);
  }

  // progress percentage
  auto v = s->get_data("/printer_state/virtual_sdcard/progress"_json_pointer);
  if (!v.is_null()) {
    int new_value = static_cast<int>(v.template get<double>() * 100);
    lv_bar_set_value(progress_bar, new_value, LV_ANIM_ON);
    lv_label_set_text(progress_label, fmt::format("{}%", new_value).c_str());
    mini_print_status.update_progress(new_value);
  }

  v = s->get_data(
      "/printer_state/gcode_move/homing_origin/2"_json_pointer);
  if (!v.is_null()) {
    z_offset.update_label(fmt::format("{:.5} mm", v.template get<double>()).c_str());
  }
}

void PrintStatusPanel::handle_metadata(const std::string &gcode_file, json &j) {
  auto eta = j["/result/estimated_time"_json_pointer];
  if (!eta.is_null()) {
    estimated_time_s = static_cast<uint32_t>(eta.template get<float>());
    spdlog::trace("updated eta {}", estimated_time_s);        

    json &v = State::get_instance()->get_data("/printer_state/print_stats/print_duration"_json_pointer);
    if (!v.is_null()) {
      uint32_t passed = static_cast<uint32_t>(v.template get<float>());
      spdlog::trace("updated time progress in handle metadata, passed {}", passed);

      std::lock_guard<std::mutex> lock(lv_lock);
      update_time_progress(passed);
    }
  }

  current_file = j["/result"_json_pointer];

  auto width_scale = (double)lv_disp_get_physical_hor_res(NULL) / 800.0;
  auto thumb_detail = KUtils::get_thumbnail(gcode_file, j, width_scale);
  std::string fullpath = thumb_detail.first;
  if (fullpath.length() > 0) {
    spdlog::trace("thumb path: {}", fullpath);
    std::lock_guard<std::mutex> lock(lv_lock);
    const std::string img_path = "A:" + fullpath;

    auto screen_width = lv_disp_get_physical_hor_res(NULL);
    // Keep the preview inside the left content column with room for its labels
    // and progress bar.
    uint32_t normalized_thumb_scale = ((0.28 * (double)screen_width) / (double)thumb_detail.second) * 256;
    lv_img_set_src(thumbnail, img_path.c_str());
    lv_img_set_zoom(thumbnail, normalized_thumb_scale);
    mini_print_status.update_img(img_path, thumb_detail.second);
  }
}


void PrintStatusPanel::consume(json &j) {
  std::lock_guard<std::mutex> lock(lv_lock);

  auto printfile = j["/params/0/print_stats/filename"_json_pointer];
  if (!printfile.is_null()) {
    // filename change indicates a start of a print
    reset();
    populate();
    foreground(); // auto move to front when print is detected
  }

  auto& pstate = j["/params/0/print_stats/state"_json_pointer];
  if (!pstate.is_null()) {
    auto print_status = pstate.template get<std::string>();
    update_status_label(print_status);
    if (print_status != "printing" && print_status != "paused") {
      mini_print_status.hide();
    } else {
      mini_print_status.show();
    }

    mini_print_status.update_status(print_status);
  }

  auto v = j["/params/0/extruder/target"_json_pointer];
  if (!v.is_null()) {
    extruder_target = v.template get<int>();
  }

  v = j["/params/0/heater_bed/target"_json_pointer];
  if (!v.is_null()) {
    heater_bed_target = v.template get<int>();
  }  
  
  v = j["/params/0/extruder/temperature"_json_pointer];
  if (!v.is_null()) {
    if (extruder_target > 0) {
      extruder_temp.update_label(fmt::format("{} / {}", v.template get<int>(), extruder_target).c_str());
    } else {
      extruder_temp.update_label(fmt::format("{}", v.template get<int>()).c_str());
    }
  }

  v = j["/params/0/heater_bed/temperature"_json_pointer];
  if (!v.is_null()) {
    if (heater_bed_target > 0) {
      bed_temp.update_label(fmt::format("{} / {}", v.template get<int>(), heater_bed_target).c_str());
    } else {
      bed_temp.update_label(fmt::format("{}", v.template get<int>()).c_str());
    }
  }

  // speed
  auto speed = j["/params/0/motion_report/live_velocity"_json_pointer];
  if (!speed.is_null()) {
    int s = static_cast<int>(speed.template get<double>());
    print_speed.update_label((std::to_string(s) + " mm/s").c_str());
  }
  
  // zoffset
  v = j["/params/0/gcode_move/homing_origin/2"_json_pointer];
  if (!v.is_null()) {
    z_offset.update_label(fmt::format("{:.5} mm", v.template get<double>()).c_str());
  }

  std::vector<std::string> values;
  for (auto &f : fan_speeds) {
    std::string fan_name = f.first;

    int fv = f.second;
    auto fan_value = j[json::json_pointer(fmt::format("/params/0/{}/value", fan_name))];
    if (!fan_value.is_null()) {
      fv = static_cast<int>(fan_value.template get<double>() * 100);
      f.second = fv;
    }

    fan_value = j[json::json_pointer(fmt::format("/params/0/{}/speed", fan_name))];
    if (!fan_value.is_null()) {
      fv = static_cast<int>(fan_value.template get<double>() * 100);
      f.second = fv;
    }
    values.push_back(fmt::format("{}%", fv));
  }

  fan0.update_label(fmt::format("{}", fmt::join(values, ", ")).c_str());

  // progress
  v = j["/params/0/print_stats/print_duration"_json_pointer];
  if (!v.is_null()) {
    uint32_t passed = static_cast<uint32_t>(v.template get<float>());
    update_time_progress(passed);
  }

  // progress percentage
  v = j["/params/0/virtual_sdcard/progress"_json_pointer];
  if (!v.is_null()) {
    int new_value = static_cast<int>(v.template get<double>() * 100);
    lv_bar_set_value(progress_bar, new_value, LV_ANIM_ON);
    lv_label_set_text(progress_label, fmt::format("{}%", new_value).c_str());
    mini_print_status.update_progress(new_value);
  }

  v = j["/params/0/motion_report/live_extruder_velocity"_json_pointer];
  if (!v.is_null()) {
    double flow = pi() / 4 * std::pow(filament_diameter, 2) * v.template get<double>();
    flow_rate.update_label(fmt::format("{:.1f} mm3/s", flow > 0.0 ? flow : 0.0).c_str());
  }

  v = j["/params/0/pause_resume/is_paused"_json_pointer];
  if (!v.is_null()) {
    bool is_paused = v.template get<bool>();
    if (is_paused) {
      resume_btn.enable();
      lv_obj_clear_flag(resume_btn.get_container(), LV_OBJ_FLAG_HIDDEN);
      
      pause_btn.disable();
      lv_obj_add_flag(pause_btn.get_container(), LV_OBJ_FLAG_HIDDEN);

    } else {
      pause_btn.enable();
      lv_obj_clear_flag(pause_btn.get_container(), LV_OBJ_FLAG_HIDDEN);      

      resume_btn.disable();
      lv_obj_add_flag(resume_btn.get_container(), LV_OBJ_FLAG_HIDDEN);
    }
  }

  // layers
  v = j["/params/0/print_stats/info"_json_pointer];
  update_layers(v);
}

void PrintStatusPanel::handle_callback(lv_event_t *event) {
  lv_obj_t *btn = lv_event_get_current_target(event);
  if (btn == back_btn.get_container()) {
    lv_obj_move_background(status_cont);

  } else if (btn == emergency_btn.get_container()) {
    ws.send_jsonrpc("printer.emergency_stop");
  } else if (btn == pause_btn.get_container()) {
    ws.send_jsonrpc("printer.print.pause");
    pause_btn.disable();

  } else if (btn == resume_btn.get_container()) {
    ws.send_jsonrpc("printer.print.resume");
    resume_btn.disable();
  } else if (btn == cancel_btn.get_container()) {
    ws.send_jsonrpc("printer.print.cancel");
  } else if (btn == finetune_btn.get_container()) {
    finetune_panel.foreground();
  } else if (btn == mini_print_status.get_container()) {
    foreground();
  }
}

void PrintStatusPanel::update_status_label(const std::string &status) {
  const char *text = "READY";
  lv_color_t color = lv_color_white();

  if (status == "printing") {
    text = "";
    color = lv_color_hex(0x4CAF50);
  } else if (status == "paused") {
    text = "PAUSED";
    color = lv_palette_main(LV_PALETTE_ORANGE);
  } else if (status == "complete") {
    text = "COMPLETE";
    color = lv_color_hex(0x4CAF50);
  } else if (status == "cancelled") {
    text = "CANCELLED";
    color = lv_palette_main(LV_PALETTE_RED);
  } else if (status == "error") {
    text = "ERROR";
    color = lv_palette_main(LV_PALETTE_RED);
  }

  lv_label_set_text(status_label, text);
  lv_obj_set_style_text_color(status_label, color, LV_PART_MAIN);
}

void PrintStatusPanel::update_clock() {
  const std::time_t now = std::time(nullptr);
  const std::tm local_time = *std::localtime(&now);
  char time_text[6] = {};
  std::strftime(time_text, sizeof(time_text), "%H:%M", &local_time);
  lv_label_set_text(time_label, time_text);
}

void PrintStatusPanel::update_time_progress(uint32_t time_passed) {
    int32_t remaining = estimated_time_s - time_passed;
    if (remaining < 0) {
      // XXX: better estimate
      time_left.update_label("...");
    } else {
      auto eta_str = KUtils::eta_string(remaining);
      time_left.update_label(eta_str.c_str());
      mini_print_status.update_eta(eta_str);
    }

    elapsed.update_label(KUtils::eta_string(time_passed).c_str());
}

void PrintStatusPanel::update_layers(json &info) {
  layers.update_label(fmt::format("{} / {}", current_layer(info), max_layer(info)).c_str());
}

int PrintStatusPanel::max_layer(json &info) {
  if (!info.is_null()) {
    auto v = info["/total_layer"_json_pointer];
    if (!v.is_null()) {
      return v.template get<int>();
    }
  }

  if (!current_file.is_null()) {
    auto v = current_file["/layer_count"_json_pointer];
    if (!v.is_null()) {
      return v.template get<int>();
    } else {
      auto first_layer_height = current_file["/first_layer_height"_json_pointer];
      auto layer_height = current_file["/layer_height"_json_pointer];
      auto object_height = current_file["/object_height"_json_pointer];

      if (!first_layer_height.is_null() && !layer_height.is_null() && !object_height.is_null()) {
        auto layer = static_cast<int>(std::ceil((object_height.template get<double>() - first_layer_height.template get<double>()) / layer_height.template get<double>() + 1));
        return layer > 0 ? layer : 0;
      }
    }
  }
  return 0;
}

int PrintStatusPanel::current_layer(json &info) {
  if (!info.is_null()) {
    auto v = info["/current_layer"_json_pointer];
    if (!v.is_null()) {
      return v.template get<int>();
    }
  }

  if (!current_file.is_null()) {
    State *s = State::get_instance();
    auto pd = s->get_data("/printer_state/print_stats/print_duration"_json_pointer);
    auto zpos = s->get_data("/printer_state/gcode_move/gcode_position/2"_json_pointer);

    auto first_layer_height = current_file["/first_layer_height"_json_pointer];
    auto layer_height = current_file["/layer_height"_json_pointer];

    if (!pd.is_null()
        && pd.template get<int>() > 0
        && !zpos.is_null()
        && !first_layer_height.is_null()
        && !layer_height.is_null()) {
      auto layer = static_cast<int>(std::ceil((zpos.template get<double>() - first_layer_height.template get<double>()) / layer_height.template get<double>() + 1));
      auto total = max_layer(info);
      if (layer > total) {
        return total;
      }

      if (layer > 0) {
        return layer;
      }
    }
  }

  return 0;
}

FineTunePanel &PrintStatusPanel::get_finetune_panel() {
  return finetune_panel;
}
