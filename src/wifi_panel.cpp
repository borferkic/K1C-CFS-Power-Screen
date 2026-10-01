#include "wifi_panel.h"
#include "powerui.h"
#include "utils.h"
#include "config.h"
#include "spdlog/spdlog.h"

#include <sstream>
#include <string>
#include <iostream>
#include <vector>
#include <utility>
#include <algorithm>

LV_IMG_DECLARE(back);

static void draw_part_event_cb(lv_event_t * e)
{
  lv_obj_t * obj = lv_event_get_target(e);
  lv_obj_draw_part_dsc_t * dsc = lv_event_get_draw_part_dsc(e);
  if(dsc->part == LV_PART_ITEMS) {
    uint32_t row = dsc->id /  lv_table_get_col_cnt(obj);
    uint32_t col = dsc->id - row * lv_table_get_col_cnt(obj);

    const char *status = lv_table_get_cell_value(obj, row, 1);
    const bool connected = status != NULL && std::string(status).find(LV_SYMBOL_OK) != std::string::npos;

    if(col == 1) {
      dsc->label_dsc->align = LV_TEXT_ALIGN_RIGHT;
    }
    if(connected) {
      if (dsc->label_dsc != NULL) {
        dsc->label_dsc->color = lv_color_hex(powerui::COLOR_ACCENT);
      }
      if (dsc->rect_dsc != NULL) {
        dsc->rect_dsc->bg_color = lv_color_hex(powerui::COLOR_ACCENT);
        dsc->rect_dsc->bg_opa = LV_OPA_10;
      }
    } else if (col == 1 && dsc->label_dsc != NULL) {
      dsc->label_dsc->color = lv_color_hex(powerui::COLOR_MUTED);
    }
  }
}

WifiPanel::WifiPanel(std::mutex &l)
  : lv_lock(l)
  , cont(lv_obj_create(lv_scr_act()))
  , spinner(lv_spinner_create(cont, 1000, 60))
  , top_cont(lv_obj_create(cont))
  , wifi_table(lv_table_create(top_cont))
  , wifi_right(lv_obj_create(top_cont))
  , prompt_cont(wifi_right)
  , wifi_label(lv_label_create(prompt_cont))
  , password_input(lv_textarea_create(prompt_cont))
  , back_btn(cont, &back, "Back", &WifiPanel::_handle_back_btn, this)
  , kb(lv_keyboard_create(cont))
{
  lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_pad_all(cont, 0, 0);
  lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
  lv_obj_add_flag(cont, LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_CLICKABLE);

  lv_obj_add_flag(spinner, LV_OBJ_FLAG_FLOATING);
  lv_obj_align(spinner, LV_ALIGN_CENTER, 0, 0);

  lv_obj_add_flag(back_btn.get_container(), LV_OBJ_FLAG_FLOATING);  
  lv_obj_align(back_btn.get_container(), LV_ALIGN_BOTTOM_RIGHT, 0, -20);
  
  lv_obj_set_flex_grow(top_cont, 1);
  lv_obj_set_flex_flow(top_cont, LV_FLEX_FLOW_ROW);
  lv_obj_clear_flag(top_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_border_width(top_cont, 0, 0);
  lv_obj_set_width(top_cont, LV_PCT(100));
  
  // ---- PowerUI look: dark page, a network list card on the left and a status / password card on the right.
  using namespace powerui;
  lv_obj_set_style_bg_color(cont, lv_color_hex(COLOR_BG), 0);
  lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(cont, 0, 0);
  lv_obj_set_style_pad_all(cont, px(12), 0);
  lv_obj_set_style_pad_row(cont, px(12), 0);
  lv_obj_set_style_bg_opa(top_cont, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(top_cont, 0, 0);
  lv_obj_set_style_pad_column(top_cont, px(12), 0);

  lv_obj_set_style_bg_color(wifi_table, lv_color_hex(COLOR_CARD), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(wifi_table, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(wifi_table, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(wifi_table, lv_color_white(), LV_PART_MAIN);
  lv_obj_set_style_border_opa(wifi_table, LV_OPA_10, LV_PART_MAIN);
  lv_obj_set_style_radius(wifi_table, px(14), LV_PART_MAIN);
  lv_obj_set_style_pad_all(wifi_table, px(6), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(wifi_table, LV_OPA_TRANSP, LV_PART_ITEMS);
  lv_obj_set_style_text_color(wifi_table, lv_color_hex(COLOR_FG), LV_PART_ITEMS);
  lv_obj_set_style_text_font(wifi_table, &lv_font_montserrat_16, LV_PART_ITEMS);
  lv_obj_set_style_pad_ver(wifi_table, px(18), LV_PART_ITEMS);
  lv_obj_set_style_pad_hor(wifi_table, px(14), LV_PART_ITEMS);
  lv_obj_set_style_border_side(wifi_table, LV_BORDER_SIDE_BOTTOM, LV_PART_ITEMS);
  lv_obj_set_style_border_width(wifi_table, 1, LV_PART_ITEMS);
  lv_obj_set_style_border_color(wifi_table, lv_color_white(), LV_PART_ITEMS);
  lv_obj_set_style_border_opa(wifi_table, LV_OPA_10, LV_PART_ITEMS);
  lv_obj_set_style_bg_color(wifi_table, lv_color_hex(COLOR_SECONDARY), LV_PART_ITEMS | LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(wifi_table, LV_OPA_COVER, LV_PART_ITEMS | LV_STATE_PRESSED);
  lv_obj_set_style_bg_color(wifi_table, lv_color_hex(COLOR_SECONDARY), LV_PART_ITEMS | LV_STATE_FOCUSED);
  lv_obj_set_style_bg_opa(wifi_table, LV_OPA_COVER, LV_PART_ITEMS | LV_STATE_FOCUSED);
  lv_obj_set_width(wifi_table, px(420));
  lv_obj_set_height(wifi_table, LV_PCT(100));
  // lv_obj_remove_style(wifi_table, NULL, LV_PART_ITEMS | LV_STATE_PRESSED);
  lv_obj_add_flag(wifi_table, LV_OBJ_FLAG_HIDDEN);

  auto screen_width = px(420) - px(12) - px(110);
  
  lv_table_set_col_width(wifi_table, 0, screen_width);
  lv_table_set_col_width(wifi_table, 1, px(110));
  
  lv_obj_add_event_cb(wifi_table, &WifiPanel::_handle_callback, LV_EVENT_VALUE_CHANGED, this);
  lv_obj_add_event_cb(wifi_table, &WifiPanel::_handle_callback, LV_EVENT_SIZE_CHANGED, this);
  lv_obj_add_event_cb(wifi_table, draw_part_event_cb, LV_EVENT_DRAW_PART_BEGIN, NULL);

  lv_obj_set_scroll_dir(wifi_table, LV_DIR_TOP | LV_DIR_BOTTOM);

  lv_obj_set_style_bg_color(wifi_right, lv_color_hex(COLOR_CARD), 0);
  lv_obj_set_style_bg_opa(wifi_right, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(wifi_right, 1, 0);
  lv_obj_set_style_border_color(wifi_right, lv_color_white(), 0);
  lv_obj_set_style_border_opa(wifi_right, LV_OPA_10, 0);
  lv_obj_set_style_radius(wifi_right, px(14), 0);
  lv_obj_set_style_pad_all(wifi_right, px(14), 0);
  lv_obj_set_height(wifi_right, LV_PCT(100));
  lv_obj_set_style_bg_opa(prompt_cont, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(prompt_cont, 0, 0);
  lv_obj_set_width(wifi_label, LV_PCT(100));
  lv_label_set_long_mode(wifi_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_color(wifi_label, lv_color_hex(COLOR_FG), 0);
  lv_obj_set_style_text_font(wifi_label, &lv_font_montserrat_18, 0);
  lv_label_set_text(wifi_label, "Select a network");
  lv_obj_set_style_bg_color(password_input, lv_color_hex(COLOR_SECONDARY), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(password_input, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_color(password_input, lv_color_hex(COLOR_ACCENT), LV_PART_MAIN | LV_STATE_FOCUSED);
  lv_obj_set_style_border_width(password_input, 1, LV_PART_MAIN);
  lv_obj_set_style_radius(password_input, px(10), LV_PART_MAIN);
  lv_obj_set_style_text_color(password_input, lv_color_hex(COLOR_FG), LV_PART_MAIN);
  lv_obj_set_style_text_font(password_input, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_set_style_bg_color(kb, lv_color_hex(COLOR_CARD), LV_PART_MAIN);
  lv_obj_set_style_bg_color(kb, lv_color_hex(COLOR_SECONDARY), LV_PART_ITEMS);
  lv_obj_set_style_text_color(kb, lv_color_hex(COLOR_FG), LV_PART_ITEMS);
  lv_obj_set_style_radius(kb, px(8), LV_PART_ITEMS);
  lv_obj_set_style_border_width(kb, 0, LV_PART_ITEMS);
  lv_obj_set_flex_grow(wifi_right, 1);
  lv_obj_add_flag(wifi_right, LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_CLICKABLE);

  lv_obj_add_flag(prompt_cont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_size(prompt_cont, LV_PCT(100), LV_PCT(100));
  lv_obj_clear_flag(prompt_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_border_width(prompt_cont, 0, 0);
  
  lv_obj_align(wifi_label, LV_ALIGN_TOP_MID, 0, 10);
  lv_obj_align(password_input, LV_ALIGN_TOP_MID, 0, 40);

  lv_obj_set_size(password_input, LV_PCT(80), LV_SIZE_CONTENT);
  lv_textarea_set_password_mode(password_input, true);
  lv_textarea_set_one_line(password_input, true);

  lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(password_input, &WifiPanel::_handle_kb_input, LV_EVENT_FOCUSED, this);
  lv_obj_add_event_cb(password_input, &WifiPanel::_handle_kb_input, LV_EVENT_DEFOCUSED, this);
  lv_obj_add_event_cb(password_input, &WifiPanel::_handle_kb_input, LV_EVENT_READY, this);

  // allow clicks on non-clickables to hide the keyboard
  lv_obj_add_event_cb(prompt_cont, &WifiPanel::_handle_kb_input, LV_EVENT_CLICKED, this);
  lv_obj_add_event_cb(wifi_label, &WifiPanel::_handle_kb_input, LV_EVENT_CLICKED, this);
  lv_obj_move_background(cont);
  lv_obj_move_foreground(spinner);

  wpa_event.register_callback("WifiPanel",
      [this](const std::string &event) { this->handle_wpa_event(event); });

  wpa_event.start();
}

WifiPanel::~WifiPanel() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

void WifiPanel::foreground() {
  spdlog::trace("wifi panel fg");
  lv_obj_move_foreground(cont);
  lv_obj_clear_flag(spinner, LV_OBJ_FLAG_HIDDEN);
  wpa_event.send_command("SCAN");
  lv_obj_add_flag(back_btn.get_container(), LV_OBJ_FLAG_HIDDEN);  // Back lives in the title bar
  powerui::overlay_open("Wi-Fi", [this]() { lv_obj_move_background(cont); });
}

void WifiPanel::handle_back_btn(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if(code == LV_EVENT_CLICKED) {
    spdlog::trace("wifi panel bg");
    lv_obj_add_flag(wifi_table, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(prompt_cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_background(cont);
  }
}

void WifiPanel::handle_callback(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);

  if(code == LV_EVENT_VALUE_CHANGED) {
    uint16_t row;
    uint16_t col;

    lv_table_get_selected_cell(wifi_table, &row, &col);
    if (row == LV_TABLE_CELL_NONE || col == LV_TABLE_CELL_NONE) {
      return;
    }

    selected_network = lv_table_get_cell_value(wifi_table, row, 0);
    if (cur_network.length() > 0 && cur_network == selected_network) {
      auto ip = KUtils::interface_ip(KUtils::get_wifi_interface());
      lv_label_set_text(wifi_label, fmt::format("Connected to network {}\nIP: {}",
						selected_network,
						ip).c_str());
      lv_obj_add_flag(password_input, LV_OBJ_FLAG_HIDDEN);

    } else if (list_networks.count(selected_network)) {
      auto nid = list_networks.find(selected_network)->second;
      wpa_event.send_command(fmt::format("SELECT_NETWORK {}", nid));
      wpa_event.send_command("SAVE_CONFIG");
    } else {
      lv_label_set_text(wifi_label, fmt::format("Enter password for {}", selected_network).c_str());
      lv_obj_clear_flag(password_input, LV_OBJ_FLAG_HIDDEN);
      lv_event_send(password_input, LV_EVENT_FOCUSED, NULL);
    }

    lv_obj_clear_flag(prompt_cont, LV_OBJ_FLAG_HIDDEN);
  }
}

void WifiPanel::handle_wpa_event(const std::string &event) {
  if (event.rfind("<3>CTRL-EVENT-SCAN-RESULTS", 0) == 0) {
    // result ready
    spdlog::trace("got scan result event");
    std::istringstream f(wpa_event.send_command("SCAN_RESULTS"));
    std::string line;
    wifi_name_db.clear();
    uint32_t index = 0;

    find_current_network();
    spdlog::trace("cur_network {}", cur_network);

    std::lock_guard<std::mutex> lock(lv_lock);
    while (std::getline(f, line)) {
      if (line.rfind("bss", 0) == 0) {
	continue;
      }

      auto wifi_parts = KUtils::split(line, '\t');
      spdlog::trace("wifi parts {}", fmt::join(wifi_parts, ", "));
      if (wifi_parts.size() == 5) {
	auto inserted = wifi_name_db.insert({wifi_parts[4], std::stoi(wifi_parts[2])});
	if (inserted.second) {
	  lv_table_set_cell_value(wifi_table, index, 0, wifi_parts[4].c_str());
	  if (cur_network != wifi_parts[4]) {
	    spdlog::trace("adding symbol");
	    lv_table_set_cell_value(wifi_table, index, 1, LV_SYMBOL_WIFI);
	  } else {
	    spdlog::trace("adding symbol with ok");
	    lv_table_set_cell_value(wifi_table, index, 1, LV_SYMBOL_OK "    " LV_SYMBOL_WIFI);
	    auto ip = KUtils::interface_ip(KUtils::get_wifi_interface());
	    lv_label_set_text(wifi_label, fmt::format("Connected to network {}\nIP: {}",
						      cur_network,
						      ip).c_str());
	    lv_obj_add_flag(password_input, LV_OBJ_FLAG_HIDDEN);
	    lv_obj_clear_flag(prompt_cont, LV_OBJ_FLAG_HIDDEN);
	  }

	  index++;
	}
      }
    }
    lv_obj_scroll_to_y(wifi_table, 0, LV_ANIM_OFF);
    lv_obj_clear_flag(wifi_table, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(spinner, LV_OBJ_FLAG_HIDDEN);
  } else if (event.rfind("<3>CTRL-EVENT-CONNECTED", 0) == 0) {
    if (find_current_network()) {
      spdlog::trace("cur_network {}", cur_network);
      std::vector<std::pair<std::string, int>> pairs;
      for (auto it = wifi_name_db.begin(); it != wifi_name_db.end(); ++it) {
	pairs.push_back(*it);
      }
      
      std::sort(pairs.begin(), pairs.end(), [=](std::pair<std::string, int>& a,
						std::pair<std::string, int>& b)
      {
	return a.second > b.second;
      });
      
      std::lock_guard<std::mutex> lock(lv_lock);

      uint32_t index = 0;
      for (const auto &wifi : pairs) {
	lv_table_set_cell_value(wifi_table, index, 0, wifi.first.c_str());
	if (cur_network != wifi.first) {
	  spdlog::trace("adding symbol");
	  lv_table_set_cell_value(wifi_table, index, 1, LV_SYMBOL_WIFI);
	} else {
	  spdlog::trace("adding symbol with ok");
	  lv_table_set_cell_value(wifi_table, index, 1, LV_SYMBOL_OK "    " LV_SYMBOL_WIFI);
	    auto ip = KUtils::interface_ip(KUtils::get_wifi_interface());
	    lv_label_set_text(wifi_label, fmt::format("Connected to network {}\nIP: {}",
						      cur_network,
						      ip).c_str());
	    lv_obj_add_flag(password_input, LV_OBJ_FLAG_HIDDEN);
	    lv_obj_clear_flag(prompt_cont, LV_OBJ_FLAG_HIDDEN);
	}
	index++;
      }

      lv_obj_scroll_to_y(wifi_table, 0, LV_ANIM_OFF);
      lv_obj_clear_flag(wifi_table, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(spinner, LV_OBJ_FLAG_HIDDEN);
    }
  }
}

void WifiPanel::handle_kb_input(lv_event_t *e)
{
  const lv_event_code_t code = lv_event_get_code(e);

  if (code == LV_EVENT_FOCUSED) {
    lv_keyboard_set_textarea(kb, password_input);
    lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
  } else if (code == LV_EVENT_DEFOCUSED) {
    lv_keyboard_set_textarea(kb, NULL);
    lv_label_set_text(wifi_label, "Select a network");
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(password_input, LV_OBJ_FLAG_HIDDEN);
  } else if (code == LV_EVENT_READY) {
    const char *password = lv_textarea_get_text(password_input);
    if (password == NULL || password[0] == 0) {
      return;
    }

    // add network, set password, save wpa
    connect(password);
    lv_textarea_set_text(password_input, "");
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(wifi_label, fmt::format("Connecting to {} ...", selected_network).c_str());
    lv_obj_clear_state(password_input, LV_STATE_FOCUSED);
    lv_obj_add_flag(password_input, LV_OBJ_FLAG_HIDDEN);
  } else if (code == LV_EVENT_CLICKED) {
      lv_obj_t *target = lv_event_get_target(e);
      if (target != kb && target != password_input) {  
        lv_event_send(password_input, LV_EVENT_DEFOCUSED, NULL);
      }
  }
}

void WifiPanel::connect(const char *password) {
  std::string nid = wpa_event.send_command("ADD_NETWORK");
  spdlog::trace("add_nework {}", nid);
  if (nid.length() > 0) {
    wpa_event.send_command(fmt::format("SET_NETWORK {} ssid {:?}", nid, selected_network));
    wpa_event.send_command(fmt::format("SET_NETWORK {} psk {:?}", nid, password));
    wpa_event.send_command(fmt::format("ENABLE_NETWORK {}", nid));
    wpa_event.send_command(fmt::format("SELECT_NETWORK {}", nid));
    wpa_event.send_command("SAVE_CONFIG");
  }
}

bool WifiPanel::find_current_network() {
  list_networks.clear();
  std::string nets = wpa_event.send_command("LIST_NETWORKS");
  spdlog::trace("nets = {}", nets);
  std::istringstream f(nets);
  std::string line;
  bool found = false;
  while (std::getline(f, line)) {
    auto wifi_parts = KUtils::split(line, '\t');
    if (wifi_parts.size() == 4 && line.find("[CURRENT]") != std::string::npos) {
	cur_network = wifi_parts[1];
	list_networks.insert({wifi_parts[1], wifi_parts[0]});
	found = true;
    }

    if (wifi_parts.size() > 1) {
      list_networks.insert({wifi_parts[1], wifi_parts[0]});
    }
  }

  return found;
}
