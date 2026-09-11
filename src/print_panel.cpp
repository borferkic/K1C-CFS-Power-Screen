#include "print_panel.h"
#include "file_panel.h"
#include "state.h"
#include "utils.h"
#include "spdlog/spdlog.h"

#include <map>
#include <sstream>

LV_IMG_DECLARE(info_img);
LV_IMG_DECLARE(print);
LV_IMG_DECLARE(back);

#define SORTED_BY_NAME 1 << 0
#define SORTED_BY_MODIFIED  1 << 1

PrintPanel::PrintPanel(KWebSocketClient &websocket, std::mutex &lock, PrintStatusPanel &ps)
  : NotifyConsumer(lock)
  , ws(websocket)
  , files_cont(lv_obj_create(lv_scr_act()))
  , prompt_cont(lv_obj_create(lv_scr_act()))
  , msgbox(lv_obj_create(prompt_cont))
  , job_btn(lv_btn_create(msgbox))
  , cancel_btn(lv_btn_create(msgbox))
  , queue_btn(lv_btn_create(msgbox))
  , left_cont(lv_obj_create(files_cont))
  , file_table_btns(lv_obj_create(left_cont))
  , refresh_btn(lv_btn_create(file_table_btns))
  , modified_sort_btn(lv_btn_create(file_table_btns))
  , az_sort_btn(lv_btn_create(file_table_btns))
  , file_grid(lv_obj_create(left_cont))
  , file_view(lv_obj_create(files_cont))
  , status_btn(file_view, &info_img, "Status", &PrintPanel::_handle_status_btn, this)
  , print_btn(file_view, &print, "Print", &PrintPanel::_handle_print_callback, this)
  , back_btn(file_view, &back, "Back", &PrintPanel::_handle_back_btn, this)
  , root("", "", 0)
  , cur_dir(&root)
  , cur_file(NULL)
  , file_panel(file_view)
  , print_status(ps)
  , sorted_by(SORTED_BY_MODIFIED)
{
  spdlog::trace("building print panel");
  lv_obj_move_background(files_cont);

  lv_obj_set_size(files_cont, LV_PCT(100), LV_PCT(100));
  lv_obj_clear_flag(files_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(files_cont, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_all(files_cont, 0, 0);

  // left side cont
  lv_obj_set_size(left_cont, LV_PCT(50), LV_PCT(100));
  lv_obj_clear_flag(left_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(left_cont, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(left_cont, 0, 0);

  // file view buttons
  lv_obj_t * label = NULL;
  
  label = lv_label_create(refresh_btn);
  lv_label_set_text(label, LV_SYMBOL_REFRESH " Reload");
  lv_obj_center(label);

  label = lv_label_create(modified_sort_btn);
  lv_label_set_text(label, LV_SYMBOL_LIST " Modified");
  lv_obj_center(label);

  label = lv_label_create(az_sort_btn);
  lv_label_set_text(label, LV_SYMBOL_LIST " A-Z");
  lv_obj_center(label);

  lv_obj_add_event_cb(refresh_btn, &PrintPanel::_handle_btns, LV_EVENT_CLICKED, this);
  lv_obj_add_event_cb(modified_sort_btn, &PrintPanel::_handle_btns, LV_EVENT_CLICKED, this);
  lv_obj_add_event_cb(az_sort_btn, &PrintPanel::_handle_btns, LV_EVENT_CLICKED, this);
  
  lv_obj_set_size(file_table_btns, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_style_pad_all(file_table_btns, 4, 0);
  lv_obj_set_style_bg_color(file_table_btns, lv_palette_darken(LV_PALETTE_GREY, 4), 0);
  lv_obj_set_style_bg_opa(file_table_btns, LV_OPA_COVER, 0);

  lv_obj_clear_flag(file_table_btns, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(file_table_btns, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(file_table_btns, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END);

  lv_obj_t *sort_buttons[] = {refresh_btn, modified_sort_btn, az_sort_btn};
  for (lv_obj_t *sort_button : sort_buttons) {
    lv_obj_set_size(sort_button, LV_PCT(30), 38);
    lv_obj_set_style_bg_color(sort_button, lv_color_hex(0x555555), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(sort_button, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(sort_button, lv_color_hex(0x444444), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(sort_button, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(sort_button, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(sort_button, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
  }

  lv_obj_set_width(file_grid, LV_PCT(100));
  lv_obj_set_height(file_grid, LV_SIZE_CONTENT);
  lv_obj_set_flex_grow(file_grid, 1);
  lv_obj_set_flex_flow(file_grid, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(file_grid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_all(file_grid, 8, LV_PART_MAIN);
  lv_obj_set_style_pad_row(file_grid, 8, LV_PART_MAIN);
  lv_obj_set_style_pad_column(file_grid, 8, LV_PART_MAIN);
  lv_obj_set_style_bg_color(file_grid, lv_palette_darken(LV_PALETTE_GREY, 4), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(file_grid, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(file_grid, 0, LV_PART_MAIN);
  lv_obj_set_scroll_dir(file_grid, LV_DIR_VER);

  lv_obj_set_size(file_view, LV_PCT(50), LV_PCT(100));
  lv_obj_clear_flag(file_view, LV_OBJ_FLAG_SCROLLABLE);

  static lv_coord_t grid_main_row_dsc[] = {LV_GRID_FR(8), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
  static lv_coord_t grid_main_col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
  lv_obj_set_grid_dsc_array(file_view, grid_main_col_dsc, grid_main_row_dsc);
  lv_obj_set_grid_cell(file_panel.get_container(), LV_GRID_ALIGN_CENTER, 0, 3, LV_GRID_ALIGN_CENTER, 0, 1);

  lv_obj_set_grid_cell(status_btn.get_container(), LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_END, 1, 1);  
  lv_obj_set_grid_cell(print_btn.get_container(), LV_GRID_ALIGN_CENTER, 1, 1, LV_GRID_ALIGN_END, 1, 1);
  lv_obj_set_grid_cell(back_btn.get_container(), LV_GRID_ALIGN_CENTER, 2, 1, LV_GRID_ALIGN_END, 1, 1);

  lv_obj_move_foreground(back_btn.get_container());
  lv_obj_move_foreground(print_btn.get_container());
  lv_obj_move_foreground(status_btn.get_container());      

  // prompt
  lv_obj_add_flag(prompt_cont, LV_OBJ_FLAG_HIDDEN);  
  lv_obj_set_size(prompt_cont, LV_PCT(100), LV_PCT(100));
  lv_obj_clear_flag(prompt_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_opa(prompt_cont, LV_OPA_70, 0);

  lv_obj_set_size(msgbox, LV_PCT(60), LV_PCT(30));
  lv_obj_set_style_border_width(msgbox, 2, 0);
  lv_obj_set_style_bg_color(msgbox, lv_palette_darken(LV_PALETTE_GREY, 1), 0);
  
  lv_obj_align(msgbox, LV_ALIGN_CENTER, 0, 0);

  lv_obj_add_event_cb(job_btn, &PrintPanel::_handle_btns, LV_EVENT_CLICKED, this);
  lv_obj_align(job_btn, LV_ALIGN_BOTTOM_MID, 0, 0);

  lv_obj_add_event_cb(cancel_btn, &PrintPanel::_handle_btns, LV_EVENT_CLICKED, this);
  lv_obj_align(cancel_btn, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

  lv_obj_add_event_cb(queue_btn, &PrintPanel::_handle_btns, LV_EVENT_CLICKED, this);
  lv_obj_align(queue_btn, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  
  label = lv_label_create(job_btn);
  lv_label_set_text(label, "View Job");
  lv_obj_center(label);

  label = lv_label_create(cancel_btn);
  lv_label_set_text(label, "Cancel");
  lv_obj_center(label);

  label = lv_label_create(queue_btn);
  lv_label_set_text(label, "Queue Job");
  lv_obj_center(label);

  label = lv_label_create(msgbox);
  lv_label_set_text(label, "Printing in progress...");
  lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 0);

  ws.register_notify_update(this);
}

PrintPanel::~PrintPanel() {
  if (files_cont != NULL) {
    lv_obj_del(files_cont);
    files_cont = NULL;
  }

  if (prompt_cont != NULL) {
    lv_obj_del(prompt_cont);
    prompt_cont = NULL;
  }
}

void PrintPanel::populate_files(json &j) {
  sorted_by = SORTED_BY_MODIFIED;
  show_dir(cur_dir, SORTED_BY_MODIFIED);
}

void PrintPanel::consume(json &j) {  
  json &pstat_state = j["/params/0/print_stats/state"_json_pointer];
  if (pstat_state.is_null()) {
    return;
  }
  
  std::lock_guard<std::mutex> lock(lv_lock);
  if(pstat_state.template get<std::string>() != "printing"
     && pstat_state.template get<std::string>() != "paused") {
    status_btn.disable();
  } else {
    status_btn.enable();
  }
}

void PrintPanel::subscribe() {
  ws.send_jsonrpc("server.files.list", R"({"root":"gcodes"})"_json, [this](json &d) {
    std::lock_guard<std::mutex> lock(lv_lock);
    std::string cur_path = cur_dir->full_path;
    root.clear();
    cur_file = NULL;
    cur_dir = NULL;

    if (d.contains("result")) {
      for (auto f : d["result"]) {
        root.add_path(KUtils::split(f["path"], '/'), f["path"], f["modified"].template get<uint32_t>());
      }
    }
    Tree *dir = root.find_path(KUtils::split(cur_path, '/'));
    // need to simply this using the directory endpoint
    cur_dir = dir;
    this->populate_files(d);
  });
}

void PrintPanel::foreground() {
  json &pstat_state = State::get_instance()
    ->get_data("/printer_state/print_stats/state"_json_pointer);
  spdlog::debug("print panel print stats {}",
		pstat_state.is_null() ? "nil" : pstat_state.template get<std::string>());
    
  if (!pstat_state.is_null()
      && pstat_state.template get<std::string>() != "printing"
      && pstat_state.template get<std::string>() != "paused") {
    status_btn.disable();
  } else {
    status_btn.enable();
  }
  
  lv_obj_move_foreground(files_cont);
}

void PrintPanel::handle_callback(lv_event_t *e) {
  (void)e;
}

void PrintPanel::show_dir(Tree *dir, uint32_t sort_type) {
  file_cards.clear();
  lv_obj_clean(file_grid);

  auto create_card = [this](Tree *node, const std::string &path, bool directory) {
    lv_obj_t *card = lv_obj_create(file_grid);
    lv_obj_set_width(card, LV_PCT(47));
    lv_obj_set_height(card, 158);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(card, &PrintPanel::_handle_file_card, LV_EVENT_CLICKED, this);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x555555), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x444444), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(card, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(card, 2, LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_set_style_border_color(card, lv_color_hex(CREALITY_GREEN), LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_set_style_radius(card, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(card, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *thumbnail = lv_img_create(card);
    lv_obj_set_size(thumbnail, LV_PCT(100), 96);
    lv_img_set_size_mode(thumbnail, LV_IMG_SIZE_MODE_VIRTUAL);
    lv_img_set_src(thumbnail, directory ? LV_SYMBOL_DIRECTORY : LV_SYMBOL_IMAGE);
    lv_obj_set_style_text_font(thumbnail, &lv_font_montserrat_32, LV_PART_MAIN);
    lv_obj_set_style_img_recolor(thumbnail,
                                 directory ? lv_color_hex(CREALITY_GREEN) : lv_color_hex(0xAAAAAA),
                                 LV_PART_MAIN);
    lv_obj_set_style_img_recolor_opa(thumbnail, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t *name_label = lv_label_create(card);
    lv_obj_set_width(name_label, LV_PCT(100));
    lv_label_set_long_mode(name_label, LV_LABEL_LONG_DOT);
    lv_label_set_text(name_label, node == NULL ? ".." : node->name.c_str());
    lv_obj_set_style_text_align(name_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_color(name_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(name_label, &lv_font_montserrat_14, LV_PART_MAIN);

    lv_obj_t *eta_label = lv_label_create(card);
    lv_obj_set_width(eta_label, LV_PCT(100));
    lv_label_set_text(eta_label, directory ? "" : "Print: --");
    lv_obj_set_style_text_align(eta_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_color(eta_label, lv_color_hex(0xB8E6B8), LV_PART_MAIN);
    lv_obj_set_style_text_font(eta_label, &lv_font_montserrat_12, LV_PART_MAIN);

    file_cards.push_back({card, thumbnail, eta_label, path, node, directory});
    if (!directory && node->contains_metadata()) {
      update_file_card(path, node->metadata);
    }
  };

  create_card(NULL, dir->full_path, true);

  bool reversed = sorted_by & sort_type;
  std::vector<Tree> sorted_files;
  if (sort_type == SORTED_BY_MODIFIED) {
    KUtils::sort_map_values<std::string, Tree>(dir->children, sorted_files, [reversed](Tree &x, Tree &y) {
	if (x.is_leaf() && !y.is_leaf()) {
	  return false;
	} else if (!x.is_leaf() && y.is_leaf()) {
	  return true;
	}

	return reversed ? x.date_modified > y.date_modified : y.date_modified > x.date_modified;
      });
  } else {
    KUtils::sort_map_values<std::string, Tree>(dir->children, sorted_files, [reversed](Tree &x, Tree &y) {
	if (x.is_leaf() && !y.is_leaf()) {
	  return false;
	} else if (!x.is_leaf() && y.is_leaf()) {
	  return true;
      }

      return reversed ? x.name > y.name : y.name > x.name;
      });
  }

  sorted_by = (sorted_by ^ sort_type) & sort_type;
  for (const auto &c : sorted_files) {
    Tree *node = dir->get_child(c.name);
    if (node != NULL) {
      create_card(node, node->full_path, !node->is_leaf());
      if (node->is_leaf() && !node->contains_metadata()) {
        request_file_metadata(node);
      }
    }
  }

  lv_obj_scroll_to_y(file_grid, 0, LV_ANIM_OFF);

  // XXX: maybe use the directory instead of file endpoint in moonraker
  cur_file = NULL;
  for (auto &c : sorted_files) {
    if (c.is_leaf()) {
      const auto &selected = dir->children.find(c.name);
      if (selected != dir->children.cend()) {
	cur_file = &selected->second;
	for (auto &card : file_cards) {
	  if (card.node == cur_file) {
	    select_file_card(card);
	    break;
	  }
	}
      }
      break;
    }
  }

}

void PrintPanel::handle_file_card(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
    return;
  }

  lv_obj_t *target = lv_event_get_current_target(event);
  for (auto &card : file_cards) {
    if (card.card != target) {
      continue;
    }

    if (card.node == NULL) {
      if (cur_dir->parent != cur_dir) {
        cur_dir = cur_dir->parent;
        show_dir(cur_dir, sorted_by);
      }
    } else if (!card.node->is_leaf()) {
      cur_dir = card.node;
      show_dir(cur_dir, sorted_by);
    } else {
      select_file_card(card);
    }
    return;
  }
}

void PrintPanel::select_file_card(FileCard &card) {
  if (card.node == NULL) {
    return;
  }

  cur_file = card.node;
  for (auto &item : file_cards) {
    if (item.card == card.card) {
      lv_obj_add_state(item.card, LV_STATE_CHECKED);
    } else {
      lv_obj_clear_state(item.card, LV_STATE_CHECKED);
    }
  }
  show_file_detail(cur_file);
}

void PrintPanel::request_file_metadata(Tree *file) {
  const std::string path = file->full_path;
  ws.send_jsonrpc("server.files.metadata",
                  json{{"filename", path}},
                  [this, path](json &j) {
                    if (!j.contains("result")) {
                      return;
                    }
                    std::lock_guard<std::mutex> lock(lv_lock);
                    for (auto &card : file_cards) {
                      if (card.path == path && card.node != NULL) {
                        card.node->set_metadata(j);
                        update_file_card(path, j);
                        return;
                      }
                    }
                  });
}

void PrintPanel::update_file_card(const std::string &path, json &metadata) {
  for (auto &card : file_cards) {
    if (card.path != path) {
      continue;
    }

    auto eta_value = metadata["/result/estimated_time"_json_pointer];
    int eta = eta_value.is_null() ? -1 : eta_value.template get<int>();
    const std::string eta_text = eta > 0
      ? fmt::format("Print: {}", KUtils::eta_string(eta))
      : "Print: --";
    lv_label_set_text(card.eta_label, eta_text.c_str());

    auto width_scale = (double)lv_disp_get_physical_hor_res(NULL) / 800.0;
    auto thumb_detail = KUtils::get_thumbnail(path, metadata, width_scale);
    if (!thumb_detail.first.empty()) {
      lv_img_set_src(card.thumbnail, ("A:" + thumb_detail.first).c_str());
      size_t thumb_width = thumb_detail.second > 0 ? thumb_detail.second : 300;
      uint32_t normalized_thumb_scale =
        (static_cast<uint32_t>(140.0 * width_scale) * 256) / thumb_width;
      lv_img_set_zoom(card.thumbnail, normalized_thumb_scale);
    }
    return;
  }
}

void PrintPanel::show_file_detail(Tree *f) {
  if (f->is_leaf()) {
    if (f->contains_metadata()) {
      file_panel.refresh_view(f->metadata, f->full_path);
    } else {
      spdlog::trace("getting metadata for {}", f->name);
      const std::string path = f->full_path;
      ws.send_jsonrpc("server.files.metadata",
                      json{{"filename", path}},
                      [this, path](json &d) { this->handle_metadata(path, d); });
    }
  }
}

void PrintPanel::handle_metadata(const std::string &path, json &j) {
  spdlog::trace("handling metadata callback");  
  if (!j.contains("result")) {
    return;
  }

  std::lock_guard<std::mutex> lock(lv_lock);
  for (auto &card : file_cards) {
    if (card.path == path && card.node != NULL) {
      card.node->set_metadata(j);
      update_file_card(path, j);
      if (cur_file == card.node) {
        file_panel.refresh_view(card.node->metadata, card.node->full_path);
      }
      return;
    }
  }
}

void PrintPanel::handle_back_btn(lv_event_t *event) {
  lv_obj_t *btn = lv_event_get_current_target(event);
  if (btn == back_btn.get_container()) {
    lv_obj_move_background(files_cont);
    print_status.background();    
  }
}

void PrintPanel::handle_print_callback(lv_event_t *event) {
  lv_event_code_t code = lv_event_get_code(event);
  if (code == LV_EVENT_CLICKED && cur_file != NULL) {

    json &pstat_state = State::get_instance()
      ->get_data("/printer_state/print_stats/state"_json_pointer);
    spdlog::debug("print panel print stats {}",
		  pstat_state.is_null() ? "nil" : pstat_state.template get<std::string>());
    
    if (!pstat_state.is_null()
	&& pstat_state.template get<std::string>() != "printing"
	&& pstat_state.template get<std::string>() != "paused") {
      spdlog::debug("printer ready to print. print file {}", cur_file->full_path);
	
      // ws.send_jsonrpc("printer.gcode.script",
      // 		    json::parse(R"({"script":"PRINT_PREPARE_CLEAR"})"));

      json fname_input = {{"filename", cur_file->full_path }};
      ws.send_jsonrpc("printer.print.start", fname_input);
      print_status.foreground();

    } else {
      lv_obj_clear_flag(prompt_cont, LV_OBJ_FLAG_HIDDEN);
      lv_obj_move_foreground(prompt_cont);
    }
  }
}

void PrintPanel::handle_status_btn(lv_event_t *event) {
  lv_event_code_t code = lv_event_get_code(event);
  if (code == LV_EVENT_CLICKED && cur_file != NULL) {
    spdlog::trace("status button clicked");
    print_status.foreground();
  }
}

void PrintPanel::handle_btns(lv_event_t *event) {
  lv_event_code_t code = lv_event_get_code(event);
  if (code == LV_EVENT_CLICKED) {
    lv_obj_t *btn = lv_event_get_current_target(event);
    if (cur_file != NULL) {
      spdlog::trace("status prompt clicked");
      if (btn == queue_btn) {
	spdlog::trace("status prompt queue clicked");
      }

      if (btn == job_btn) {
	spdlog::trace("status prompt job clicked");
      }

      if (btn == cancel_btn) {
	spdlog::trace("status prompt cancel clicked");
	lv_obj_move_background(prompt_cont);
	lv_obj_add_flag(prompt_cont, LV_OBJ_FLAG_HIDDEN);
      }
    }

    if (btn == refresh_btn) {
      subscribe();
      
    } else if (btn == modified_sort_btn) {
      show_dir(cur_dir, SORTED_BY_MODIFIED);

    } else if (btn == az_sort_btn) {
      show_dir(cur_dir, SORTED_BY_NAME);
    }
  }
}
