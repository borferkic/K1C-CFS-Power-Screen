#include "macros_panel.h"
#include "powerui.h"
#include "state.h"
#include "utils.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cctype>

namespace {

std::string to_lower(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return std::tolower(c); });
  return text;
}

void style_keyboard(lv_obj_t *kb) {
  lv_obj_set_style_text_font(kb, &lv_font_montserrat_16, LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(kb, lv_color_hex(powerui::COLOR_CARD), LV_PART_MAIN);
  lv_obj_set_style_bg_color(kb, lv_color_hex(powerui::COLOR_SECONDARY), LV_PART_ITEMS);
  lv_obj_set_style_text_color(kb, lv_color_hex(powerui::COLOR_FG), LV_PART_ITEMS);
  lv_obj_set_style_radius(kb, powerui::px(8), LV_PART_ITEMS);
  lv_obj_set_style_border_width(kb, 0, LV_PART_ITEMS);
  lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
}

void style_field(lv_obj_t *field) {
  lv_obj_set_style_bg_color(field, lv_color_hex(powerui::COLOR_SECONDARY), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(field, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(field, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(field, lv_color_hex(powerui::COLOR_WHITE), LV_PART_MAIN);
  lv_obj_set_style_border_opa(field, LV_OPA_10, LV_PART_MAIN);
  lv_obj_set_style_border_color(field, lv_color_hex(powerui::COLOR_ACCENT), LV_PART_MAIN | LV_STATE_FOCUSED);
  lv_obj_set_style_border_opa(field, LV_OPA_50, LV_PART_MAIN | LV_STATE_FOCUSED);
  lv_obj_set_style_radius(field, powerui::px(8), LV_PART_MAIN);
  lv_obj_set_style_text_color(field, lv_color_hex(powerui::COLOR_FG), LV_PART_MAIN);
  lv_obj_set_style_text_font(field, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_set_style_text_color(field, lv_color_hex(powerui::COLOR_MUTED), LV_PART_TEXTAREA_PLACEHOLDER);
  lv_obj_clear_flag(field, LV_OBJ_FLAG_SCROLLABLE);
}

// Shows the keyboard for the focused field and hides it when the field loses focus or the keyboard closes.
void keyboard_for_field(lv_event_t *e, lv_obj_t *kb) {
  const lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_FOCUSED) {
    lv_keyboard_set_textarea(kb, lv_event_get_target(e));
    lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
  } else if (code == LV_EVENT_DEFOCUSED || code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
  }
}

// Parameter card of a macro that takes parameters.
struct ParamsDialog {
  KWebSocketClient *ws;
  std::string name;
  std::vector<std::pair<std::string, lv_obj_t *>> fields;
};

void params_deleted(lv_event_t *e) {
  delete (ParamsDialog *)lv_obj_get_user_data(lv_event_get_target(e));
}

void params_overlay_clicked(lv_event_t *e) {
  lv_obj_t *overlay = lv_event_get_current_target(e);
  // Only a touch on the dimmed background (not on the card) closes the dialog.
  if (lv_event_get_target(e) == overlay) {
    lv_obj_del_async(overlay);
  }
}

void params_close_clicked(lv_event_t *e) {
  lv_obj_del_async((lv_obj_t *)e->user_data);
}

void params_run_clicked(lv_event_t *e) {
  lv_obj_t *overlay = (lv_obj_t *)e->user_data;
  ParamsDialog *data = (ParamsDialog *)lv_obj_get_user_data(overlay);
  if (data != NULL) {
    std::string script = data->name;
    for (const auto &field : data->fields) {
      const char *value = lv_textarea_get_text(field.second);
      if (value != NULL && value[0] != '\0') {
        script += " " + field.first + "=" + value;
      }
    }
    spdlog::trace("sending macro: {}", script);
    data->ws->gcode_script(script);
  }
  lv_obj_del_async(overlay);
}

void params_field_event(lv_event_t *e) {
  keyboard_for_field(e, (lv_obj_t *)e->user_data);
}

}  // namespace

MacrosPanel::MacrosPanel(KWebSocketClient &c, std::mutex &l, lv_obj_t *parent)
  : ws(c)
  , lv_lock(l)
  , cont(lv_obj_create(parent))
  , search(NULL)
  , show_hidden_switch(NULL)
  , grid(NULL)
  , kb(NULL)
{
  using namespace powerui;

  style_overlay_root(cont);
  lv_obj_set_style_pad_all(cont, px(12), 0);
  lv_obj_set_style_pad_row(cont, px(10), 0);
  lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);

  // Top bar: search and the "Show hidden" switch.
  lv_obj_t *bar = plain(cont);
  lv_obj_set_size(bar, LV_PCT(100), px(40));
  lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(bar, px(10), 0);

  search = lv_textarea_create(bar);
  lv_textarea_set_one_line(search, true);
  lv_textarea_set_placeholder_text(search, "Search macros");
  lv_obj_set_height(search, px(36));
  lv_obj_set_flex_grow(search, 1);
  style_field(search);
  lv_obj_add_event_cb(search, &MacrosPanel::_handle_search_event, LV_EVENT_ALL, this);

  label(bar, "Show hidden", &lv_font_montserrat_12, lv_color_hex(COLOR_MUTED));
  show_hidden_switch = lv_switch_create(bar);
  style_switch(show_hidden_switch);
  lv_obj_add_event_cb(show_hidden_switch, &MacrosPanel::_handle_show_hidden, LV_EVENT_VALUE_CHANGED, this);

  // Tiles: three columns, scrolls vertically.
  grid = lv_obj_create(cont);
  lv_obj_remove_style_all(grid);
  lv_obj_set_width(grid, LV_PCT(100));
  lv_obj_set_flex_grow(grid, 1);
  lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_style_pad_row(grid, px(10), 0);
  lv_obj_set_style_pad_column(grid, px(10), 0);
  lv_obj_set_scroll_dir(grid, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(grid, LV_SCROLLBAR_MODE_OFF);

  kb = lv_keyboard_create(cont);
  lv_obj_set_size(kb, LV_PCT(100), px(190));
  style_keyboard(kb);
}

MacrosPanel::~MacrosPanel()
{
  macro_items.clear();
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

void MacrosPanel::populate() {
  macro_items.clear();
  lv_obj_clean(grid);

  auto &config_json = State::get_instance()
    ->get_data("/printer_state/configfile/config"_json_pointer);
  auto &macro_settings = State::get_instance()->get_data("/powerscreensettings/macros/settings"_json_pointer);

  const lv_coord_t pad = powerui::px(12);
  const lv_coord_t gap = powerui::px(10);
  const lv_coord_t tile_width = (powerui::overlay_width_px() - 2 * pad - 2 * gap - 2) / 3;

  if (!config_json.is_null()) {
    auto macros = KUtils::parse_macros(config_json);
    for (auto const & [name, params] : macros) {
      bool hidden = false;
      auto known = hidden_state.find(name);
      if (known != hidden_state.end()) {
        hidden = known->second;
      } else {
        const json::json_pointer path("/" + name + "/hidden");
        if (macro_settings.is_object() && macro_settings.contains(path) && macro_settings.at(path).is_boolean()) {
          hidden = macro_settings.at(path).template get<bool>();
        }
        hidden_state[name] = hidden;
      }

      macro_items.push_back(std::make_shared<MacroItem>(
        grid, name, params, tile_width, hidden,
        [this](MacroItem &item) {
          // SAVE_CONFIG and the restarts always ask first.
          if (needs_confirmation(item.name())) {
            confirm_run(item);
          } else {
            run_or_open(item);
          }
        },
        [this](MacroItem &item) { open_options(item); }));
    }
  }

  if (macro_items.empty()) {
    powerui::label(grid, "No macros found", &lv_font_montserrat_14, lv_color_hex(powerui::COLOR_MUTED));
  }
  apply_filter();
}

bool MacrosPanel::needs_confirmation(const std::string &name) {
  const std::string upper = [&name]() {
    std::string text = name;
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return std::toupper(c); });
    return text;
  }();
  return upper.find("SAVE_CONFIG") != std::string::npos || upper.find("RESTART") != std::string::npos;
}

void MacrosPanel::run_or_open(MacroItem &item) {
  if (item.params().empty()) {
    spdlog::trace("sending macro: {}", item.name());
    ws.gcode_script(item.name());
  } else {
    open_params(item);
  }
}

void MacrosPanel::confirm_run(MacroItem &item) {
  const std::string name = item.name();
  const bool save_config = name.find("SAVE_CONFIG") != std::string::npos;
  std::string text = save_config ? "The configuration will be saved and Klipper will restart."
                                 : "The printer service or firmware will restart.";
  auto &pstate = State::get_instance()->get_data("/printer_state/print_stats/state"_json_pointer);
  if (pstate.is_string()) {
    const std::string state = pstate.template get<std::string>();
    if (state == "printing" || state == "paused") {
      text += " This will cancel the current print.";
    }
  }
  // Look the tile up again when confirmed: the list may have been rebuilt while the card was open.
  powerui::confirm_dialog(("Run " + name + "?").c_str(), text.c_str(), "Run", powerui::ActionKind::Destructive,
                          [this, name]() {
    for (const auto &candidate : macro_items) {
      if (candidate->name() == name) {
        run_or_open(*candidate);
        return;
      }
    }
  });
}

void MacrosPanel::apply_filter() {
  const std::string text = to_lower(lv_textarea_get_text(search));
  const bool show_hidden = lv_obj_has_state(show_hidden_switch, LV_STATE_CHECKED);
  for (const auto &item : macro_items) {
    const bool matches = text.empty() || to_lower(item->name()).find(text) != std::string::npos;
    item->update_visibility(matches, show_hidden);
  }
}

void MacrosPanel::handle_search_event(lv_event_t *e) {
  const lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_VALUE_CHANGED) {
    apply_filter();
  } else {
    keyboard_for_field(e, kb);
  }
}

void MacrosPanel::handle_show_hidden(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
    apply_filter();
  }
}

void MacrosPanel::open_params(MacroItem &item) {
  using namespace powerui;

  lv_obj_t *overlay = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(overlay);
  lv_obj_set_size(overlay, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(overlay, lv_color_hex(COLOR_BLACK), 0);
  lv_obj_set_style_bg_opa(overlay, LV_OPA_60, 0);
  lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);
  ParamsDialog *data = new ParamsDialog{&ws, item.name(), {}};
  lv_obj_set_user_data(overlay, data);
  lv_obj_add_event_cb(overlay, &params_deleted, LV_EVENT_DELETE, NULL);
  lv_obj_add_event_cb(overlay, &params_overlay_clicked, LV_EVENT_CLICKED, NULL);

  lv_obj_t *dialog_kb = lv_keyboard_create(overlay);
  lv_obj_set_size(dialog_kb, LV_PCT(100), px(190));
  lv_obj_align(dialog_kb, LV_ALIGN_BOTTOM_MID, 0, 0);
  style_keyboard(dialog_kb);

  const int w = 460, pad = 16, top = 54, row_h = 40, gap = 6;
  const int rows = (int)item.params().size();
  const int visible_rows = std::min(rows, 3);
  const int list_h = visible_rows * row_h + (visible_rows - 1) * gap;
  const int h = top + list_h + 12 + 46 + 14;

  lv_obj_t *dlg = card(overlay, 0, 0, w, h);
  lv_obj_align(dlg, LV_ALIGN_TOP_MID, 0, px(8));
  lv_obj_set_style_border_opa(dlg, LV_OPA_30, 0);

  lv_obj_t *title = label(dlg, item.name().c_str(), &lv_font_montserrat_18, lv_color_hex(COLOR_FG));
  lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
  lv_obj_set_width(title, px(w - 2 * pad - 44));
  lv_obj_set_pos(title, px(pad), px(19));
  action_button(dlg, NULL, LV_SYMBOL_CLOSE, ActionKind::Outline, w - pad - 32, 12, 32, 32, &params_close_clicked,
                overlay);

  lv_obj_t *list = lv_obj_create(dlg);
  lv_obj_remove_style_all(list);
  lv_obj_set_pos(list, px(pad), px(top));
  lv_obj_set_size(list, px(w - 2 * pad), px(list_h));
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(list, px(gap), 0);
  lv_obj_set_scroll_dir(list, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);

  for (auto const & [key, value] : item.params()) {
    lv_obj_t *row = plain(list);
    lv_obj_set_size(row, LV_PCT(100), px(row_h));
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, px(8), 0);

    lv_obj_t *name = label(row, key.c_str(), &lv_font_montserrat_14, lv_color_hex(COLOR_MUTED));
    lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
    lv_obj_set_width(name, LV_PCT(38));

    lv_obj_t *field = lv_textarea_create(row);
    lv_textarea_set_one_line(field, true);
    lv_textarea_set_text(field, value.c_str());
    lv_obj_set_height(field, px(36));
    lv_obj_set_flex_grow(field, 1);
    style_field(field);
    lv_obj_add_event_cb(field, &params_field_event, LV_EVENT_ALL, dialog_kb);
    data->fields.push_back({key, field});
  }

  action_button(dlg, NULL, "Run", ActionKind::Primary, pad, top + list_h + 12, w - 2 * pad, 46, &params_run_clicked,
                overlay);
}

void MacrosPanel::open_options(MacroItem &item) {
  const std::string name = item.name();
  const bool hide = !item.is_hidden();
  powerui::choice_dialog("Macro options", {hide ? "Hide macro" : "Show macro"},
                         [this, name, hide](int) { set_macro_hidden(name, hide); });
}

void MacrosPanel::set_macro_hidden(const std::string &name, bool hide) {
  hidden_state[name] = hide;
  for (const auto &item : macro_items) {
    if (item->name() == name) {
      item->set_hidden(hide);
    }
  }
  apply_filter();

  json setting = {
    {"namespace", "powerscreen"},
    {"key", "macros.settings." + name},
    {"value", {{"hidden", hide}}}
  };
  ws.send_jsonrpc("server.database.post_item", setting);
}
