#include "console_panel.h"
#include "powerui.h"
#include "state.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cctype>

LV_FONT_DECLARE(dejavusans_mono_14);

ConsolePanel::ConsolePanel(KWebSocketClient &websocket_client, std::mutex &lock, lv_obj_t *parent)
  : ws(websocket_client)
  , lv_lock(lock)
  , console_cont(lv_obj_create(parent))
  , top_cont(lv_obj_create(console_cont))
  , output(lv_textarea_create(top_cont))
  , macro_list(lv_table_create(top_cont))
  , input_cont(lv_obj_create(console_cont))
  , input(lv_textarea_create(input_cont))
  , kb(lv_keyboard_create(console_cont))
{
  lv_obj_align(console_cont, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_size(console_cont, LV_PCT(100), LV_PCT(100));
  lv_obj_set_flex_flow(console_cont, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(console_cont, 0, 0);
  lv_obj_set_style_text_font(console_cont, &dejavusans_mono_14, LV_STATE_DEFAULT);

  lv_obj_set_flex_grow(top_cont, 1);
  lv_obj_set_style_pad_all(top_cont, 0, 0);
  lv_obj_set_width(top_cont, LV_PCT(100));

  lv_obj_set_style_border_width(output, 0, 0);
  lv_obj_set_size(output, LV_PCT(60), LV_PCT(100));
  lv_obj_set_style_border_width(output, 0, LV_STATE_FOCUSED | LV_PART_CURSOR);

  lv_obj_set_flex_grow(input, 1);
  lv_obj_set_width(input, LV_PCT(100));
  lv_textarea_set_one_line(input, true);
  lv_textarea_set_cursor_click_pos(output, false);

  lv_obj_set_flex_flow(input_cont, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_all(input_cont, 0, 0);
  lv_obj_set_size(input_cont, LV_PCT(100), LV_SIZE_CONTENT);

  lv_obj_t *send_btn = lv_btn_create(input_cont);
  lv_obj_set_style_text_font(send_btn, &lv_font_montserrat_16, LV_STATE_DEFAULT);
  lv_obj_set_width(send_btn, 100);
  lv_obj_t *send_btn_label = lv_label_create(send_btn);
  lv_label_set_text(send_btn_label, LV_SYMBOL_NEW_LINE);
  lv_obj_center(send_btn_label);
  lv_obj_add_event_cb(send_btn , &ConsolePanel::_handle_send_macro, LV_EVENT_CLICKED, this);

  lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_text_font(kb, &lv_font_montserrat_16, LV_STATE_DEFAULT);
  lv_obj_add_event_cb(input, &ConsolePanel::_handle_kb_input, LV_EVENT_ALL, this);

  lv_obj_set_size(macro_list, LV_PCT(40), LV_PCT(100));
  lv_obj_align(macro_list, LV_ALIGN_TOP_RIGHT, 0, 0);
  lv_table_set_col_width(macro_list, 0, LV_PCT(100));

  lv_obj_add_event_cb(macro_list, &ConsolePanel::_handle_select_macro, LV_EVENT_ALL, this);
  lv_obj_set_scroll_dir(macro_list, LV_DIR_TOP | LV_DIR_BOTTOM);

  lv_obj_t *label = lv_label_create(input);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_16, LV_STATE_DEFAULT);
  lv_label_set_text(label, "      " LV_SYMBOL_CLOSE "      ");
  lv_obj_align(label, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_add_flag(label, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(label, &ConsolePanel::_handle_clear_input, LV_EVENT_CLICKED, this);

  // ---- PowerUI console (reference 11-consola): one card with a colored log, a row of shortcuts and the input.
  const lv_color_t bg = lv_color_hex(powerui::COLOR_BG);
  const lv_color_t card_bg = lv_color_hex(powerui::COLOR_CARD);
  const lv_color_t fg = lv_color_hex(powerui::COLOR_FG);
  const lv_color_t muted = lv_color_hex(powerui::COLOR_MUTED);
  const lv_color_t accent = lv_color_hex(powerui::COLOR_ACCENT);
  auto border = [](lv_obj_t *obj, lv_part_t part) {
    lv_obj_set_style_border_width(obj, 1, part);
    lv_obj_set_style_border_color(obj, lv_color_hex(powerui::COLOR_WHITE), part);
    lv_obj_set_style_border_opa(obj, LV_OPA_10, part);
  };

  lv_obj_set_style_bg_color(console_cont, bg, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(console_cont, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(console_cont, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(console_cont, 12, LV_PART_MAIN);
  lv_obj_set_style_pad_row(console_cont, 0, LV_PART_MAIN);
  lv_obj_clear_flag(console_cont, LV_OBJ_FLAG_SCROLLABLE);

  card = lv_obj_create(console_cont);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_width(card, LV_PCT(100));
  lv_obj_set_flex_grow(card, 1);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_bg_color(card, card_bg, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
  border(card, LV_PART_MAIN);
  lv_obj_set_style_radius(card, 14, LV_PART_MAIN);
  lv_obj_set_style_pad_all(card, 16, LV_PART_MAIN);
  lv_obj_set_style_pad_row(card, 12, LV_PART_MAIN);
  lv_obj_move_to_index(card, 0);

  // Log box: dark, monospaced, colored lines, Clear button in the corner.
  lv_obj_set_parent(top_cont, card);
  lv_obj_set_width(top_cont, LV_PCT(100));
  lv_obj_set_flex_grow(top_cont, 1);
  lv_obj_set_style_bg_color(top_cont, bg, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(top_cont, LV_OPA_COVER, LV_PART_MAIN);
  border(top_cont, LV_PART_MAIN);
  lv_obj_set_style_radius(top_cont, 10, LV_PART_MAIN);
  lv_obj_set_style_pad_all(top_cont, 12, LV_PART_MAIN);
  lv_obj_add_flag(output, LV_OBJ_FLAG_HIDDEN);        // the old text area; the log is a colored label now
  lv_obj_add_flag(macro_list, LV_OBJ_FLAG_HIDDEN);    // replaced by the shortcut chips
  log_box = top_cont;
  lv_obj_add_flag(log_box, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(log_box, LV_DIR_VER);
  lv_obj_set_flex_flow(log_box, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(log_box, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);  // short logs hug the bottom
  log_label = lv_label_create(log_box);
  lv_label_set_recolor(log_label, true);
  lv_label_set_long_mode(log_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(log_label, LV_PCT(100));
  lv_label_set_text(log_label, "");
  lv_obj_set_style_text_font(log_label, &dejavusans_mono_14, LV_PART_MAIN);
  lv_obj_set_style_text_color(log_label, fg, LV_PART_MAIN);

  lv_obj_t *clear_btn = lv_btn_create(log_box);
  lv_obj_add_flag(clear_btn, LV_OBJ_FLAG_FLOATING);
  lv_obj_set_size(clear_btn, 88, 32);
  lv_obj_align(clear_btn, LV_ALIGN_TOP_RIGHT, 0, 0);
  lv_obj_set_style_bg_color(clear_btn, lv_color_hex(powerui::COLOR_SECONDARY), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(clear_btn, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(clear_btn, 8, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(clear_btn, 0, LV_PART_MAIN);
  lv_obj_t *clear_label = lv_label_create(clear_btn);
  lv_label_set_text(clear_label, LV_SYMBOL_CLOSE "  Clear");
  lv_obj_set_style_text_font(clear_label, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_set_style_text_color(clear_label, fg, LV_PART_MAIN);
  lv_obj_center(clear_label);
  lv_obj_add_event_cb(clear_btn, &ConsolePanel::_handle_clear_log, LV_EVENT_CLICKED, this);

  // Shortcut chips: command history and the macros that match what is typed.
  chips_row = lv_obj_create(card);
  lv_obj_remove_style_all(chips_row);
  lv_obj_set_size(chips_row, LV_PCT(100), 36);
  lv_obj_set_flex_flow(chips_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(chips_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(chips_row, 8, LV_PART_MAIN);
  lv_obj_add_flag(chips_row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(chips_row, LV_DIR_HOR);
  lv_obj_set_scrollbar_mode(chips_row, LV_SCROLLBAR_MODE_OFF);

  // Input and Send.
  lv_obj_set_parent(input_cont, card);
  lv_obj_set_width(input_cont, LV_PCT(100));
  lv_obj_set_height(input_cont, 44);
  lv_obj_set_style_bg_opa(input_cont, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(input_cont, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_column(input_cont, 12, LV_PART_MAIN);
  lv_obj_clear_flag(input_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_align(input_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_set_height(input, 44);
  lv_obj_set_style_bg_color(input, card_bg, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(input, LV_OPA_COVER, LV_PART_MAIN);
  border(input, LV_PART_MAIN);
  lv_obj_set_style_border_color(input, accent, LV_PART_MAIN | LV_STATE_FOCUSED);
  lv_obj_set_style_border_opa(input, LV_OPA_50, LV_PART_MAIN | LV_STATE_FOCUSED);
  lv_obj_set_style_radius(input, 10, LV_PART_MAIN);
  lv_obj_set_style_text_color(input, fg, LV_PART_MAIN);
  lv_textarea_set_placeholder_text(input, "Send G-code...");
  lv_obj_set_style_text_color(input, muted, LV_PART_TEXTAREA_PLACEHOLDER);

  lv_obj_set_size(send_btn, 110, 44);
  lv_obj_set_style_bg_color(send_btn, lv_color_hex(powerui::COLOR_PRIMARY), LV_PART_MAIN);
  lv_obj_set_style_bg_color(send_btn, lv_color_hex(powerui::COLOR_PRIMARY_PRESSED), LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(send_btn, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(send_btn, 10, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(send_btn, 0, LV_PART_MAIN);
  lv_label_set_text(send_btn_label, LV_SYMBOL_OK "  Send");
  lv_obj_set_style_text_color(send_btn_label, lv_color_hex(powerui::COLOR_WHITE), LV_PART_MAIN);

  lv_obj_set_parent(kb, console_cont);
  lv_obj_set_style_bg_color(kb, card_bg, LV_PART_MAIN);
  lv_obj_set_style_bg_color(kb, lv_color_hex(powerui::COLOR_SECONDARY), LV_PART_ITEMS);
  lv_obj_set_style_text_color(kb, fg, LV_PART_ITEMS);
  lv_obj_set_style_radius(kb, 8, LV_PART_ITEMS);
  lv_obj_set_style_border_width(kb, 0, LV_PART_ITEMS);
  refresh_chips("");

  // ws.register_gcode_resp([this](json& d) { this->handle_macro_response(d); });
  ws.register_method_callback("notify_gcode_response",
			      "ConsolePanel",
			      [this](json& d) { this->handle_macro_response(d); });
}

ConsolePanel::~ConsolePanel() {
  if (console_cont != NULL) {
    lv_obj_del(console_cont);
    console_cont = NULL;
  }
}

void ConsolePanel::set_back_callback(std::function<void()> callback) {
  back_callback = callback;
}

void ConsolePanel::handle_back(lv_event_t *e) {
  (void)e;
  if (back_callback) {
    back_callback();
  }
}

lv_obj_t *ConsolePanel::get_container() {
  return console_cont;
}

void ConsolePanel::handle_kb_input(lv_event_t *e)
{
  const lv_event_code_t code = lv_event_get_code(e);

  if(code == LV_EVENT_FOCUSED) {
    lv_keyboard_set_textarea(kb, input);
    lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
  }
  
  if(code == LV_EVENT_DEFOCUSED) {
    lv_keyboard_set_textarea(kb, NULL);
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
  }

  if (code == LV_EVENT_VALUE_CHANGED) {
    // filter the shortcut chips with the typed text
    refresh_chips(std::string(lv_textarea_get_text(input)));
  }

  if (code == LV_EVENT_READY) {
    spdlog::debug("keyboard ready");
    const char *cmd = lv_textarea_get_text(input);
    if (cmd == NULL || cmd[0] == 0) {
      return;
    }

    append_log(std::string("> ") + cmd);
    ws.gcode_script(cmd);

    if (!history.empty()) {
      const auto &front = history.front();
      
      if (front != std::string(cmd)) {
	if (history.size() >= 20) {
	  history.pop_back();
	}
	history.push_front(cmd);

	json h = {
	  {"namespace", "fluidd"}, // leverage history from fluidd
	  {"key", "console.commandHistory"},
	  {"value", history}
	};

	ws.send_jsonrpc("server.database.post_item", h);
      }
    }
		    
    lv_textarea_set_text(input, "");
    refresh_chips("");
  }
}

void ConsolePanel::handle_select_macro(lv_event_t *e) {
  (void)e;  // the table was replaced by the shortcut chips
}

void ConsolePanel::handle_macros(json &j) {
  // TODO: this is a race condition
  auto &db_history = State::get_instance()->get_data("/console/commandHistory"_json_pointer);

  if (!db_history.is_null()) {
    history = db_history.template get<std::list<std::string>>();
  }

  if (j.contains("result")) {
    const auto &m = j["result"];
    for (const auto &el : m.items()) {
      all_macros.push_back(el.key());
    }
  }

  std::lock_guard<std::mutex> lock(lv_lock);
  refresh_chips("");
}

// A shortcut: history first, then macros, filtered by the typed prefix.
void ConsolePanel::refresh_chips(const std::string &prefix) {
  lv_obj_clean(chips_row);
  std::string upper;
  std::transform(prefix.begin(), prefix.end(), std::back_inserter(upper), [](unsigned char ch) { return std::toupper(ch); });
  int count = 0;
  auto add_chip = [this, &count](const std::string &text) {
    if (count >= 16) {
      return;
    }
    lv_obj_t *chip = lv_btn_create(chips_row);
    lv_obj_set_height(chip, 36);
    lv_obj_set_width(chip, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(chip, lv_color_hex(powerui::COLOR_SECONDARY), LV_PART_MAIN);
    lv_obj_set_style_bg_color(chip, lv_color_hex(powerui::COLOR_PRESSED), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(chip, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(chip, 8, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(chip, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(chip, 14, LV_PART_MAIN);
    lv_obj_t *chip_label = lv_label_create(chip);
    lv_label_set_text(chip_label, text.c_str());
    lv_obj_set_style_text_font(chip_label, &dejavusans_mono_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(chip_label, lv_color_hex(powerui::COLOR_FG), LV_PART_MAIN);
    lv_obj_center(chip_label);
    lv_obj_add_event_cb(chip, &ConsolePanel::_handle_chip, LV_EVENT_CLICKED, this);
    ++count;
  };
  auto matches = [&](const std::string &m) { return prefix.empty() || m.rfind(upper, 0) == 0 || m.rfind(prefix, 0) == 0; };
  for (const auto &m : history) {
    if (matches(m)) {
      add_chip(m);
    }
  }
  for (const auto &m : all_macros) {
    if (matches(m)) {
      add_chip(m);
    }
  }
  lv_obj_scroll_to_x(chips_row, 0, LV_ANIM_OFF);
}

void ConsolePanel::handle_chip(lv_event_t *e) {
  lv_obj_t *chip = lv_event_get_current_target(e);
  lv_obj_t *chip_label = lv_obj_get_child(chip, 0);
  if (chip_label != NULL) {
    lv_textarea_set_text(input, lv_label_get_text(chip_label));
  }
}

void ConsolePanel::handle_clear_log(lv_event_t *e) {
  (void)e;
  log_lines.clear();
  refresh_log();
}

// Log lines are colored by kind: commands white, comments muted, ok green, errors red.
void ConsolePanel::append_log(const std::string &line) {
  log_lines.push_back(line);
  while (log_lines.size() > 150) {
    log_lines.pop_front();
  }
  refresh_log();
}

void ConsolePanel::refresh_log() {
  std::string text;
  for (const auto &line : log_lines) {
    std::string escaped;
    for (char ch : line) {
      if (ch == '#') {
        escaped += "##";
      } else {
        escaped += ch;
      }
    }
    std::string lower;
    std::transform(line.begin(), line.end(), std::back_inserter(lower), [](unsigned char ch) { return std::tolower(ch); });
    const char *color = "FAFAFA";
    if (line.rfind("//", 0) == 0) {
      color = "A1A1A1";
    } else if (line.rfind("!!", 0) == 0 || lower.find("error") != std::string::npos) {
      color = "FF6467";
    } else if (line.rfind("ok", 0) == 0) {
      color = "4ADE80";
    }
    text += std::string("#") + color + " " + escaped + "#\n";
  }
  lv_label_set_text(log_label, text.c_str());
  lv_obj_update_layout(log_box);
  const lv_coord_t hidden_below = lv_obj_get_scroll_bottom(log_box);
  if (hidden_below > 0 && lv_obj_get_height(log_box) > 40) {
    lv_obj_scroll_by(log_box, 0, -hidden_below, LV_ANIM_OFF);  // follow the newest line
  }
}

void ConsolePanel::handle_macro_response(json &j) {
  if (j.contains("params")) {
    std::lock_guard<std::mutex> lock(lv_lock);
    for (auto &l : j["params"]) {
      append_log(l.template get<std::string>());
    }
  }
}

void ConsolePanel::handle_send_macro(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_CLICKED) {
    lv_event_send(input, LV_EVENT_READY, this);
  }
}

void ConsolePanel::handle_clear_input(lv_event_t *e) {
  lv_textarea_set_text(input, "");
}
