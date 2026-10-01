#include "bedmesh_panel.h"
#include "powerui.h"
#include "state.h"
#include "spdlog/spdlog.h"

#include <vector>
#include <cmath>
#include <algorithm>
#include <string>

LV_IMG_DECLARE(back);
LV_IMG_DECLARE(delete_img);
LV_IMG_DECLARE(bedmesh_img);
LV_IMG_DECLARE(sd_img);


static lv_color_t color_gradient(double offset);

BedMeshPanel::BedMeshPanel(KWebSocketClient &c, std::mutex &l)
  : NotifyConsumer(l)
  , ws(c)
  , cont(lv_obj_create(lv_scr_act()))
  , prompt(lv_obj_create(lv_scr_act()))
  , top_cont(lv_obj_create(cont))
  , mesh_table(lv_table_create(top_cont))
  , profile_cont(lv_table_create(top_cont))
  , profile_table(lv_table_create(profile_cont))
  , profile_info(lv_table_create(profile_cont))
  , controls_cont(lv_obj_create(cont))
  , save_btn(controls_cont, &sd_img, "Save Profile", &BedMeshPanel::_handle_callback, this)
  , clear_btn(controls_cont, &delete_img, "Clear Profile", &BedMeshPanel::_handle_callback, this)
  , calibrate_btn(controls_cont, &bedmesh_img, "Calibrate", &BedMeshPanel::_handle_callback, this)
  , back_btn(controls_cont, &back, "Back", &BedMeshPanel::_handle_callback, this)
  , msgbox(lv_obj_create(prompt))
  , input(lv_textarea_create(msgbox))
  , kb(lv_keyboard_create(prompt))
{
  lv_obj_move_background(cont);
  
  lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_pad_all(cont, 0, 0);
  lv_obj_set_style_pad_row(cont, 0, 0);
  
  lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_grow(top_cont, 1);

  lv_obj_set_flex_align(top_cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_width(top_cont, LV_PCT(100));
  lv_obj_clear_flag(top_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(top_cont, LV_FLEX_FLOW_ROW);

  auto screen_width = powerui::overlay_width_px();
  if (screen_width < 800) {
    lv_obj_set_style_text_font(mesh_table, &lv_font_montserrat_8, LV_STATE_DEFAULT);
  } else {
    lv_obj_set_style_text_font(mesh_table, &lv_font_montserrat_10, LV_STATE_DEFAULT);
  }
  auto scale = (double)screen_width / 800.0;
  auto hscale = powerui::overlay_height_scale();
  
  lv_obj_set_size(profile_cont, LV_PCT(50), 340 * hscale);
  lv_obj_set_style_pad_all(profile_cont, 0, 0);
  lv_obj_set_style_border_width(profile_cont, 0, 0);  
  lv_obj_clear_flag(profile_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(profile_cont, LV_FLEX_FLOW_COLUMN);

  // profile table
  lv_table_set_col_width(profile_table, 0, 260 * scale);
  lv_table_set_col_width(profile_table, 1, 50 * scale);
  lv_table_set_col_width(profile_table, 2, 50 * scale);
  lv_obj_set_height(profile_table, 200 * hscale);

  // profile info
  lv_table_set_col_width(profile_info, 0, 240 * scale);
  lv_table_set_col_width(profile_info, 1, 120 * scale);
  lv_obj_set_height(profile_info, 150 * hscale);
  lv_obj_set_style_pad_top(profile_info, 5, LV_PART_ITEMS | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_bottom(profile_info, 5, LV_PART_ITEMS | LV_STATE_DEFAULT);
  lv_obj_set_style_border_side(profile_info, LV_BORDER_SIDE_BOTTOM, 0);

  // button controls
  lv_obj_set_width(controls_cont, LV_PCT(100));
  lv_obj_set_flex_align(controls_cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_flex_flow(controls_cont, LV_FLEX_FLOW_ROW);
  lv_obj_clear_flag(controls_cont, LV_OBJ_FLAG_SCROLLABLE);
 
  lv_obj_add_event_cb(mesh_table, &BedMeshPanel::_mesh_draw_cb, LV_EVENT_DRAW_PART_BEGIN, this);
  lv_obj_add_event_cb(profile_table, &BedMeshPanel::_handle_profile_action, LV_EVENT_VALUE_CHANGED, this);

  // prompt
  lv_obj_set_style_pad_all(prompt, 0, 0);
  lv_obj_set_size(prompt, LV_PCT(100), LV_PCT(100));
  lv_obj_clear_flag(prompt, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(prompt, LV_OBJ_FLAG_HIDDEN);  
  lv_obj_set_style_bg_opa(prompt, LV_OPA_70, 0);

  lv_textarea_set_one_line(input, true);
  lv_obj_set_width(input, LV_PCT(100));
  
  // lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_flex_flow(msgbox, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(msgbox, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(msgbox, 25, 0);
  
  lv_obj_clear_flag(msgbox, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_set_size(msgbox, LV_PCT(60), LV_PCT(40));
  lv_obj_set_style_border_width(msgbox, 2, 0);
  lv_obj_set_style_bg_color(msgbox, lv_palette_darken(LV_PALETTE_GREY, 1), 0);
  lv_obj_align(msgbox, LV_ALIGN_TOP_MID, 0, 20);

  lv_obj_t * label = NULL;

  label = lv_label_create(msgbox);
  lv_obj_set_width(label, LV_PCT(100));
  lv_label_set_text(label, "Saving the profile will restart the printer.");

  lv_obj_t *prompt_save_btn = lv_btn_create(msgbox);
  lv_obj_t *prompt_cancel_btn = lv_btn_create(msgbox);

  label = lv_label_create(prompt_save_btn);
  lv_label_set_text(label, "Save");

  label = lv_label_create(prompt_cancel_btn);
  lv_label_set_text(label, "Cancel");
  lv_obj_center(label);

  lv_obj_add_event_cb(prompt_save_btn, &BedMeshPanel::_handle_prompt_save, LV_EVENT_CLICKED, this);

  lv_obj_add_event_cb(prompt_cancel_btn, &BedMeshPanel::_handle_prompt_cancel, LV_EVENT_CLICKED, this);

  lv_obj_add_event_cb(input, &BedMeshPanel::_handle_kb_input, LV_EVENT_ALL, this);
  lv_keyboard_set_textarea(kb, input);

  lv_obj_move_background(prompt);

  // PowerUI layout for the overlay area (736 x 440): heat map card, profiles card and an action row.
  {
    using namespace powerui;
    lv_obj_set_layout(cont, 0);
    lv_obj_set_style_radius(cont, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(cont, lv_color_hex(COLOR_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(cont, 0, LV_PART_MAIN);

    lv_obj_t *mesh_card = card(cont, 12, 12, 400, 320);
    lv_obj_t *mesh_title = powerui::label(mesh_card, "Bed mesh", &lv_font_montserrat_16, lv_color_hex(COLOR_FG));
    lv_obj_set_pos(mesh_title, px(20), px(16));
    mesh_profile_label = powerui::label(mesh_card, "", &lv_font_montserrat_14, lv_color_hex(COLOR_MUTED));
    lv_obj_set_pos(mesh_profile_label, px(20), px(40));
    mesh_stats_label = powerui::label(mesh_card, "", &lv_font_montserrat_14, lv_color_hex(COLOR_MUTED));
    lv_obj_align(mesh_stats_label, LV_ALIGN_BOTTOM_LEFT, px(20), -px(16));

    lv_obj_set_parent(mesh_table, mesh_card);
    lv_obj_align(mesh_table, LV_ALIGN_TOP_MID, 0, px(68));
    lv_obj_set_style_bg_opa(mesh_table, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(mesh_table, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(mesh_table, px(3), LV_PART_ITEMS);
    lv_obj_set_style_border_width(mesh_table, 1, LV_PART_ITEMS);
    lv_obj_set_style_border_color(mesh_table, lv_color_hex(COLOR_CARD), LV_PART_ITEMS);
    lv_obj_set_style_text_color(mesh_table, lv_color_hex(COLOR_FG), LV_PART_ITEMS);

    lv_obj_t *profile_card = card(cont, 424, 12, 300, 320);
    lv_obj_t *profile_title = powerui::label(profile_card, "Profiles", &lv_font_montserrat_16, lv_color_hex(COLOR_FG));
    lv_obj_set_pos(profile_title, px(20), px(16));
    lv_obj_set_parent(profile_cont, profile_card);
    lv_obj_set_size(profile_cont, px(276), px(256));
    lv_obj_set_pos(profile_cont, px(10), px(48));
    lv_obj_set_style_bg_opa(profile_cont, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(profile_cont, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(profile_cont, 0, LV_PART_MAIN);

    for (lv_obj_t *table : {profile_table, profile_info}) {
      lv_obj_set_style_bg_opa(table, LV_OPA_TRANSP, LV_PART_MAIN);
      lv_obj_set_style_border_width(table, 0, LV_PART_MAIN);
      lv_obj_set_style_bg_opa(table, LV_OPA_TRANSP, LV_PART_ITEMS);
      lv_obj_set_style_text_color(table, lv_color_hex(COLOR_FG), LV_PART_ITEMS);
      lv_obj_set_style_border_side(table, LV_BORDER_SIDE_BOTTOM, LV_PART_ITEMS);
      lv_obj_set_style_border_width(table, 1, LV_PART_ITEMS);
      lv_obj_set_style_border_color(table, lv_color_white(), LV_PART_ITEMS);
      lv_obj_set_style_border_opa(table, LV_OPA_10, LV_PART_ITEMS);
      lv_obj_set_style_pad_top(table, px(7), LV_PART_ITEMS);
      lv_obj_set_style_pad_bottom(table, px(7), LV_PART_ITEMS);
    }
    lv_table_set_col_width(profile_table, 0, px(168));
    lv_table_set_col_width(profile_table, 1, px(54));
    lv_table_set_col_width(profile_table, 2, px(54));
    lv_obj_set_height(profile_table, px(116));
    lv_table_set_col_width(profile_info, 0, px(150));
    lv_table_set_col_width(profile_info, 1, px(126));
    lv_obj_set_height(profile_info, px(130));
    lv_obj_set_style_text_font(profile_info, &lv_font_montserrat_12, LV_PART_ITEMS);

    lv_obj_add_flag(top_cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(controls_cont, LV_OBJ_FLAG_HIDDEN);

    // Action row: three buttons across the full width at y 344.
    auto put = [this](ButtonContainer &button, int x, int w) {
      lv_obj_set_parent(button.get_container(), cont);
      button.set_fixed_size(powerui::px(w), powerui::px(84));
      lv_obj_set_pos(button.get_container(), powerui::px(x), powerui::px(344));
    };
    put(calibrate_btn, 12, 229);
    put(save_btn, 253, 230);
    put(clear_btn, 494, 229);
    style_button(calibrate_btn.get_container(), calibrate_btn.get_button(), ButtonKind::Soft);
    style_button(save_btn.get_container(), save_btn.get_button(), ButtonKind::Outline);
    style_button(clear_btn.get_container(), clear_btn.get_button(), ButtonKind::Destructive);
  }

  ws.register_notify_update(this);
}

BedMeshPanel::~BedMeshPanel() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }

  if (prompt != NULL) {
    lv_obj_del(prompt);
    prompt = NULL;
  }
}

void BedMeshPanel::consume(json &j) {
  auto bm = j["/params/0/bed_mesh"_json_pointer];
  if (!bm.is_null()) {
    spdlog::trace("bedmesh panel consume {}",  bm["/profiles"_json_pointer].dump());
    refresh_views_with_lock(bm);
  }
}

void BedMeshPanel::refresh_views_with_lock(json &bm) {
  std::lock_guard<std::mutex> lock(lv_lock); // more grandular?
  refresh_views(bm);
}

void BedMeshPanel::refresh_views(json &bm) {  
  if (!bm.is_null()) {
    auto active_profile_j = bm["/profile_name"_json_pointer];
    size_t row_idx = 0;
    if(active_profile_j.is_null()) {
      save_btn.disable();
      return;
    }

    active_profile = active_profile_j.template get<std::string>();
    if (active_profile.length() > 0) {
      save_btn.enable();

      refresh_profile_info(active_profile);
      lv_obj_clear_flag(mesh_table, LV_OBJ_FLAG_HIDDEN);
      
      auto mesh_json = bm["/probed_matrix"_json_pointer];
      if (mesh_json.is_null()) {
	mesh_json = State::get_instance()->get_data("/printer_state/bed_mesh/probed_matrix"_json_pointer);
      }

      mesh = mesh_json.template get<std::vector<std::vector<double>>>();

      if (mesh_profile_label != NULL) {
        lv_label_set_text(mesh_profile_label, active_profile.c_str());
      }
      if (mesh_stats_label != NULL && !mesh.empty()) {
        double lowest = 1e9, highest = -1e9;
        for (const auto &mesh_row : mesh) {
          for (double value : mesh_row) {
            lowest = std::min(lowest, value);
            highest = std::max(highest, value);
          }
        }
        lv_label_set_text(mesh_stats_label,
                          fmt::format("Min {:.3f}   Max {:.3f}   Range {:.3f} mm", lowest, highest, highest - lowest).c_str());
      }

      // calculate cell width
      if (mesh.size() > 0 && mesh[0].size() > 0) {
	auto scale = powerui::overlay_width_scale();
	int col_width = std::max(4, (int)(370 * scale / mesh[0].size()));
	int cel_height = std::max(1, (int)(col_width / 2 - 8));

	lv_obj_set_style_pad_top(mesh_table, cel_height, LV_PART_ITEMS | LV_STATE_DEFAULT);
	lv_obj_set_style_pad_bottom(mesh_table, cel_height, LV_PART_ITEMS | LV_STATE_DEFAULT);

	lv_table_set_col_cnt(mesh_table, mesh[0].size());
	for (int i = 0; i < mesh[0].size(); i++) {
	  lv_table_set_col_width(mesh_table, i, col_width);
	}
      }

      for (int i = mesh.size() - 1; i >= 0; i--) {
	auto row = mesh[i];
	for (int j = 0; j < row.size(); j++) {
	  if (mesh.size() < 6 && row.size() < 6) {
	    lv_table_set_cell_value(mesh_table, row_idx, j, fmt::format("{:.2f}", row[j]).c_str());
	  } else {
	    lv_table_set_cell_value(mesh_table, row_idx, j, "");
	  }
	}
	row_idx++;
      }
      lv_table_set_row_cnt(mesh_table, row_idx);
    } else {
      // no active profile, hide mesh matrix
      lv_obj_add_flag(mesh_table, LV_OBJ_FLAG_HIDDEN);
      save_btn.disable();
    }

    // populate profiles tables
    auto profiles = bm["/profiles"_json_pointer];
    if (profiles.is_null()) {
      profiles = State::get_instance()->get_data("/printer_state/bed_mesh/profiles"_json_pointer);
    }

    spdlog::trace("active {}, profiles is {}", active_profile, profiles.dump());

    if (profiles.size() > 0) {
      lv_obj_clear_flag(profile_cont, LV_OBJ_FLAG_HIDDEN);
      
      row_idx = 0;
      for (auto &el : profiles.items()) {
	lv_table_clear_cell_ctrl(profile_table, row_idx, 0, LV_TABLE_CELL_CTRL_MERGE_RIGHT);
      
	bool is_active = el.key() == active_profile;
	if (is_active) {
	  lv_table_add_cell_ctrl(profile_table, row_idx, 0, LV_TABLE_CELL_CTRL_MERGE_RIGHT);
	  lv_table_set_cell_value(profile_table, row_idx, 1, "");
	} else {
	  lv_table_set_cell_value(profile_table, row_idx, 1, LV_SYMBOL_UPLOAD);
	}

	lv_table_set_cell_value(profile_table, row_idx, 0, el.key().c_str());
	lv_table_set_cell_value(profile_table, row_idx, 2, LV_SYMBOL_CLOSE);
	row_idx++;
      }
      lv_table_set_row_cnt(profile_table, row_idx);
    } else {
      // no profiles, hide profile table
      lv_obj_add_flag(profile_cont, LV_OBJ_FLAG_HIDDEN);      
    }
  }  
}

void BedMeshPanel::refresh_profile_info(std::string profile) {
  auto mesh_params = State::get_instance()->get_data(json::json_pointer(
			fmt::format("/printer_state/bed_mesh/profiles/{}/mesh_params", profile)));
  spdlog::trace("refreshing profile info {}, {}", profile, mesh_params.dump());

  if (!mesh_params.is_null()) {
    uint32_t rowidx = 0;
    auto &v = mesh_params["/algo"_json_pointer];
    if (!v.is_null()) {
      lv_table_set_cell_value(profile_info, rowidx, 0, "Algorithm");
      lv_table_set_cell_value(profile_info, rowidx, 1, v.template get<std::string>().c_str());
      rowidx++;
    }

    v = mesh_params["/tension"_json_pointer];
    if (!v.is_null()) {
      lv_table_set_cell_value(profile_info, rowidx, 0, "Tension");
      lv_table_set_cell_value(profile_info, rowidx, 1, fmt::format("{}", v.template get<double>()).c_str());
      rowidx++;
    }    

    std::vector<int> xvalues;
    for (auto &param : {"min_x", "max_x", "x_count", "mesh_x_pps"}) {
      v = mesh_params[json::json_pointer(fmt::format("/{}", param))];
      if (!v.is_null()) {
	xvalues.push_back(v.template get<int>());
      }
    }

    lv_table_set_cell_value(profile_info, rowidx, 0, "X (min, max, count, pps)");
    lv_table_set_cell_value(profile_info, rowidx, 1, fmt::format("{}", fmt::join(xvalues, ", ")).c_str());
    rowidx++;    

    std::vector<int> yvalues;
    for (auto &param : {"min_y", "max_y", "y_count", "mesh_y_pps"}) {
      v = mesh_params[json::json_pointer(fmt::format("/{}", param))];
      if (!v.is_null()) {
	yvalues.push_back(v.template get<int>());
      }
    }

    lv_table_set_cell_value(profile_info, rowidx, 0, "Y (min, max, count, pps)");
    lv_table_set_cell_value(profile_info, rowidx, 1, fmt::format("{}", fmt::join(xvalues, ", ")).c_str());
    rowidx++;
  }
}

void BedMeshPanel::foreground() {
  auto bm = State::get_instance()->get_data("/printer_state/bed_mesh"_json_pointer);
  spdlog::trace("bm {}", bm.dump());
  refresh_views(bm);
  
  lv_obj_move_foreground(cont);
  lv_obj_add_flag(back_btn.get_container(), LV_OBJ_FLAG_HIDDEN);  // Back lives in the title bar
  powerui::overlay_open("Bed Mesh", [this]() { lv_obj_move_background(cont); });
}

void BedMeshPanel::handle_callback(lv_event_t *event) {
  lv_obj_t *btn = lv_event_get_current_target(event);
  if (btn == save_btn.get_container()) {
    spdlog::trace("mesh save pressed");
    lv_obj_clear_flag(prompt, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);

    if (active_profile.length() > 0) {
      lv_textarea_set_text(input, active_profile.c_str());
    }
    
    lv_obj_move_foreground(prompt);
    
  } else if (btn == clear_btn.get_container()) {
    spdlog::trace("mesh clear pressed");
    ws.gcode_script("BED_MESH_CLEAR");
    
  } else if (btn == calibrate_btn.get_container()) {
    spdlog::trace("mesh calibrate pressed");
    auto v = State::get_instance()
      ->get_data("/printer_state/toolhead/homed_axes"_json_pointer);
    if (!v.is_null()) {
      if (v.template get<std::string>() == "xy") {
	ws.gcode_script("BED_MESH_CALIBRATE");
	return;
      }
    }
    ws.gcode_script("G28 X Y Z\nBED_MESH_CALIBRATE");

  } else if (btn == back_btn.get_container()) {
    spdlog::trace("back button pressed");
    lv_obj_move_background(cont);
  }
}

void BedMeshPanel::handle_profile_action(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if(code == LV_EVENT_VALUE_CHANGED) {
    uint16_t row;
    uint16_t col;

    lv_table_get_selected_cell(profile_table, &row, &col);
    uint16_t row_count = lv_table_get_row_cnt(profile_table);
    if (row == LV_TABLE_CELL_NONE || col == LV_TABLE_CELL_NONE || row >= row_count) {
      return;
    }
    const char *selected = lv_table_get_cell_value(profile_table, row, col);    
    const char *profile_name = lv_table_get_cell_value(profile_table, row, 0);
    spdlog::trace("selected {}, {}, value {}", row, col, profile_name);
    if (col == 2) {
      // delete profile
      spdlog::trace("delete mesh {}", profile_name);
      ws.gcode_script(fmt::format("BED_MESH_PROFILE REMOVE=\"{}\"\nSAVE_CONFIG", profile_name));
    } else if (col == 1 && selected != NULL && strlen(selected) != 0) {
      // load profile
      spdlog::trace("selected {}, load mesh {}", strlen(selected), profile_name);
      ws.gcode_script(fmt::format("BED_MESH_PROFILE LOAD=\"{}\"", profile_name));

      // // populate profile info
      // refresh_profile_info(profile_name);
    } else if (col == 0) {
      // display mesh info and refresh bed mesh matrix
    }
      
  }
}

void BedMeshPanel::handle_prompt_save(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if(code == LV_EVENT_CLICKED) {
    const char *profile_name = lv_textarea_get_text(input);
    if (profile_name == NULL || strlen(profile_name) == 0) {
      return;
    }

    ws.gcode_script(fmt::format("BED_MESH_PROFILE SAVE=\"{}\"\nSAVE_CONFIG", profile_name));

    lv_textarea_set_text(input, "");
    lv_obj_add_flag(prompt, LV_OBJ_FLAG_HIDDEN);  
    lv_obj_move_background(prompt);
  }  
}

void BedMeshPanel::handle_prompt_cancel(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if(code == LV_EVENT_CLICKED) {
    lv_obj_add_flag(prompt, LV_OBJ_FLAG_HIDDEN);  
    lv_obj_move_background(prompt);
  }
}


void BedMeshPanel::handle_kb_input(lv_event_t *e)
{
  const lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_READY) {
    const char *profile_name = lv_textarea_get_text(input);
    if (profile_name == NULL || strlen(profile_name) == 0) {
      return;
    }

    ws.gcode_script(fmt::format("BED_MESH_PROFILE SAVE=\"{}\"\nSAVE_CONFIG", profile_name));

    lv_textarea_set_text(input, "");
    lv_obj_add_flag(prompt, LV_OBJ_FLAG_HIDDEN);  
    lv_obj_move_background(prompt);
  }
}

void BedMeshPanel::mesh_draw_cb(lv_event_t * e)
{
  lv_obj_t * obj = lv_event_get_target(e);
  lv_obj_draw_part_dsc_t * dsc = lv_event_get_draw_part_dsc(e);
  if(dsc->part == LV_PART_ITEMS && !mesh.empty()) {
    uint32_t row = dsc->id /  lv_table_get_col_cnt(obj);
    uint32_t col = dsc->id - row * lv_table_get_col_cnt(obj);

    dsc->label_dsc->align = LV_TEXT_ALIGN_CENTER;
    dsc->label_dsc->color = lv_color_hex(0xFAFAFA);
    
    // rows of the mesh is reversed
    int32_t reversed_row_idx = mesh.size() - row - 1;
    double offset = mesh[reversed_row_idx][col];
    lv_color_t color = color_gradient(offset);

    dsc->rect_dsc->bg_color = color;
    dsc->rect_dsc->bg_opa = LV_OPA_COVER;
  }
}

static lv_color_t color_gradient(double offset)
{
  const double full_scale = 0.25;  // mm of deviation that reaches the full color
  const double t = std::min(1.0, std::abs(offset) / full_scale);
  const lv_color_t neutral = lv_color_hex(0x3A3A3A);
  const lv_color_t target = offset > 0 ? lv_color_hex(0xF87171) : lv_color_hex(0x60A5FA);
  return lv_color_mix(target, neutral, static_cast<uint8_t>(t * 255));
}
