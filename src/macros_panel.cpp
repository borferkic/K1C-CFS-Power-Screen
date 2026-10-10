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

// Ids of the special pills of the group row.
const std::string OTHER_ID = "\x01";
const std::string ADD_ID = "\x02";

std::string trim(const std::string &text) {
  const size_t first = text.find_first_not_of(" \t");
  if (first == std::string::npos) {
    return "";
  }
  const size_t last = text.find_last_not_of(" \t");
  return text.substr(first, last - first + 1);
}

// Card with a single text field (group name). `on_confirm` returns an error text to show and keep the card open,
// or an empty string to accept and close.
struct NameDialog {
  std::function<std::string(const std::string &)> on_confirm;
  lv_obj_t *field;
  lv_obj_t *error;
};

void name_deleted(lv_event_t *e) {
  delete (NameDialog *)lv_obj_get_user_data(lv_event_get_target(e));
}

void name_overlay_clicked(lv_event_t *e) {
  lv_obj_t *overlay = lv_event_get_current_target(e);
  if (lv_event_get_target(e) == overlay) {
    lv_obj_del_async(overlay);
  }
}

void name_close_clicked(lv_event_t *e) {
  lv_obj_del_async((lv_obj_t *)e->user_data);
}

void name_confirm_clicked(lv_event_t *e) {
  lv_obj_t *overlay = (lv_obj_t *)e->user_data;
  NameDialog *data = (NameDialog *)lv_obj_get_user_data(overlay);
  if (data == NULL) {
    return;
  }
  const std::string error = data->on_confirm(trim(lv_textarea_get_text(data->field)));
  if (!error.empty()) {
    lv_label_set_text(data->error, error.c_str());
    return;
  }
  lv_obj_del_async(overlay);
}

void name_field_event(lv_event_t *e) {
  keyboard_for_field(e, (lv_obj_t *)e->user_data);
}

void name_dialog(const char *title, const std::string &initial, const char *confirm_text,
                 std::function<std::string(const std::string &)> on_confirm) {
  using namespace powerui;

  lv_obj_t *overlay = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(overlay);
  lv_obj_set_size(overlay, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(overlay, lv_color_hex(COLOR_BLACK), 0);
  lv_obj_set_style_bg_opa(overlay, LV_OPA_60, 0);
  lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);
  NameDialog *data = new NameDialog{std::move(on_confirm), NULL, NULL};
  lv_obj_set_user_data(overlay, data);
  lv_obj_add_event_cb(overlay, &name_deleted, LV_EVENT_DELETE, NULL);
  lv_obj_add_event_cb(overlay, &name_overlay_clicked, LV_EVENT_CLICKED, NULL);

  lv_obj_t *dialog_kb = lv_keyboard_create(overlay);
  lv_obj_set_size(dialog_kb, LV_PCT(100), px(190));
  lv_obj_align(dialog_kb, LV_ALIGN_BOTTOM_MID, 0, 0);
  style_keyboard(dialog_kb);

  const int w = 460, pad = 16, h = 182;
  lv_obj_t *dlg = card(overlay, 0, 0, w, h);
  lv_obj_align(dlg, LV_ALIGN_TOP_MID, 0, px(8));
  lv_obj_set_style_border_opa(dlg, LV_OPA_30, 0);

  lv_obj_t *t = label(dlg, title, &lv_font_montserrat_18, lv_color_hex(COLOR_FG));
  lv_obj_set_pos(t, px(pad), px(19));
  action_button(dlg, NULL, LV_SYMBOL_CLOSE, ActionKind::Outline, w - pad - 32, 12, 32, 32, &name_close_clicked,
                overlay);

  data->field = lv_textarea_create(dlg);
  lv_textarea_set_one_line(data->field, true);
  lv_textarea_set_max_length(data->field, 16);
  lv_textarea_set_placeholder_text(data->field, "Group name");
  lv_textarea_set_text(data->field, initial.c_str());
  lv_obj_set_pos(data->field, px(pad), px(54));
  lv_obj_set_size(data->field, px(w - 2 * pad), px(40));
  style_field(data->field);
  lv_obj_add_event_cb(data->field, &name_field_event, LV_EVENT_ALL, dialog_kb);

  data->error = label(dlg, "", &lv_font_montserrat_12, lv_color_hex(COLOR_DESTRUCTIVE));
  lv_obj_set_pos(data->error, px(pad), px(100));

  action_button(dlg, NULL, confirm_text, ActionKind::Primary, pad, 122, w - 2 * pad, 46, &name_confirm_clicked,
                overlay);

  // The keyboard is ready as soon as the card opens.
  lv_keyboard_set_textarea(dialog_kb, data->field);
  lv_obj_clear_flag(dialog_kb, LV_OBJ_FLAG_HIDDEN);
}

}  // namespace

MacrosPanel::MacrosPanel(KWebSocketClient &c, std::mutex &l, lv_obj_t *parent)
  : ws(c)
  , lv_lock(l)
  , cont(lv_obj_create(parent))
  , search(NULL)
  , show_hidden_switch(NULL)
  , pill_row(NULL)
  , grid(NULL)
  , kb(NULL)
  , groups_loaded(false)
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

  // Group pills: All, the user's groups, Other and "+"; scrolls sideways.
  pill_row = lv_obj_create(cont);
  lv_obj_remove_style_all(pill_row);
  lv_obj_set_size(pill_row, LV_PCT(100), px(32));
  lv_obj_set_flex_flow(pill_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(pill_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(pill_row, px(8), 0);
  lv_obj_set_scroll_dir(pill_row, LV_DIR_HOR);
  lv_obj_set_scrollbar_mode(pill_row, LV_SCROLLBAR_MODE_OFF);

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
  load_groups();

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
  rebuild_pills();
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
    const bool matches = (text.empty() || to_lower(item->name()).find(text) != std::string::npos)
      && in_selected_group(item->name());
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
  std::vector<std::string> options = {hide ? "Hide macro" : "Show macro", "Move to group"};
  if (assignment.count(name) > 0) {
    options.push_back("Remove from group");
  }
  powerui::choice_dialog("Macro options", options, [this, name, hide](int index) {
    if (index == 0) {
      set_macro_hidden(name, hide);
    } else if (index == 1) {
      open_move_menu(name);
    } else if (index == 2) {
      assign_macro(name, "");
    }
  });
}

bool MacrosPanel::in_selected_group(const std::string &macro) const {
  if (selected.empty()) {
    return true;
  }
  const auto found = assignment.find(macro);
  if (selected == OTHER_ID) {
    return found == assignment.end();
  }
  return found != assignment.end() && found->second == selected;
}

void MacrosPanel::load_groups() {
  if (groups_loaded) {
    return;
  }
  groups_loaded = true;
  auto &saved = State::get_instance()->get_data("/powerscreensettings/macros/groups"_json_pointer);
  if (!saved.is_object()) {
    return;
  }
  if (saved.contains("list") && saved.at("list").is_array()) {
    for (const auto &entry : saved.at("list")) {
      if (entry.is_string() && (int)groups.size() < MAX_GROUPS) {
        groups.push_back(entry.template get<std::string>());
      }
    }
  }
  if (saved.contains("assign") && saved.at("assign").is_object()) {
    for (const auto &entry : saved.at("assign").items()) {
      if (!entry.value().is_string()) {
        continue;
      }
      const std::string group = entry.value().template get<std::string>();
      if (std::find(groups.begin(), groups.end(), group) != groups.end()) {
        assignment[entry.key()] = group;
      }
    }
  }
}

void MacrosPanel::save_groups() {
  json value;
  value["list"] = groups;
  value["assign"] = assignment;
  json setting = {
    {"namespace", "powerscreen"},
    {"key", "macros.groups"},
    {"value", value}
  };
  ws.send_jsonrpc("server.database.post_item", setting);
}

void MacrosPanel::schedule_rebuild_pills() {
  // The pill that was touched is deleted by the rebuild, so wait until its event is over.
  lv_async_call([](void *panel) { static_cast<MacrosPanel *>(panel)->rebuild_pills(); }, this);
}

void MacrosPanel::rebuild_pills() {
  using namespace powerui;

  bool has_ungrouped = false;
  for (const auto &item : macro_items) {
    if (assignment.count(item->name()) == 0) {
      has_ungrouped = true;
    }
  }
  const bool show_other = !groups.empty() && has_ungrouped;
  if ((selected == OTHER_ID && !show_other)
      || (!selected.empty() && selected != OTHER_ID
          && std::find(groups.begin(), groups.end(), selected) == groups.end())) {
    selected.clear();
  }

  lv_obj_clean(pill_row);
  pill_ids.clear();

  auto add_pill = [this](const std::string &id, const std::string &text) {
    const bool is_add = id == ADD_ID;
    const bool is_selected = !is_add && id == selected;
    lv_obj_t *pill = lv_obj_create(pill_row);
    lv_obj_remove_style_all(pill);
    lv_obj_set_size(pill, LV_SIZE_CONTENT, px(32));
    lv_obj_set_style_pad_hor(pill, px(14), 0);
    lv_obj_set_style_radius(pill, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(pill, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(pill, 1, 0);
    if (is_selected) {
      lv_obj_set_style_bg_color(pill, lv_color_hex(COLOR_ACCENT_BG), 0);
      lv_obj_set_style_border_color(pill, lv_color_hex(COLOR_ACCENT), 0);
      lv_obj_set_style_border_opa(pill, LV_OPA_50, 0);
    } else {
      lv_obj_set_style_bg_color(pill, lv_color_hex(is_add ? COLOR_CARD : COLOR_SECONDARY), 0);
      lv_obj_set_style_border_color(pill, lv_color_hex(COLOR_WHITE), 0);
      lv_obj_set_style_border_opa(pill, LV_OPA_10, 0);
    }
    lv_obj_clear_flag(pill, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(pill, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_flex_flow(pill, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(pill, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *text_label = label(pill, text.c_str(), &lv_font_montserrat_14,
                                 lv_color_hex(is_selected || is_add ? COLOR_ACCENT : COLOR_FG));
    lv_obj_clear_flag(text_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(pill, &MacrosPanel::_handle_pill_event, LV_EVENT_ALL, this);
    pill_ids.push_back(id);
  };

  add_pill("", "All");
  for (const auto &group : groups) {
    add_pill(group, group);
  }
  if (show_other) {
    add_pill(OTHER_ID, "Other");
  }
  if ((int)groups.size() < MAX_GROUPS) {
    add_pill(ADD_ID, "+");
  }
}

void MacrosPanel::handle_pill_event(lv_event_t *e) {
  const lv_event_code_t code = lv_event_get_code(e);
  if (code != LV_EVENT_SHORT_CLICKED && code != LV_EVENT_LONG_PRESSED) {
    return;
  }
  const uint32_t index = lv_obj_get_index(lv_event_get_current_target(e));
  if (index >= pill_ids.size()) {
    return;
  }
  const std::string id = pill_ids[index];
  if (code == LV_EVENT_SHORT_CLICKED) {
    if (id == ADD_ID) {
      open_new_group("");
    } else {
      selected = id;
      apply_filter();
      schedule_rebuild_pills();
    }
  } else if (!id.empty() && id != OTHER_ID && id != ADD_ID) {
    open_group_options(id);
  }
}

std::string MacrosPanel::validate_group_name(const std::string &name, const std::string &ignore) const {
  if (name.empty()) {
    return "Enter a name";
  }
  const std::string lower = to_lower(name);
  if (lower == "all" || lower == "other") {
    return "That name is reserved";
  }
  for (const auto &group : groups) {
    if (group != ignore && to_lower(group) == lower) {
      return "That group already exists";
    }
  }
  if (ignore.empty() && (int)groups.size() >= MAX_GROUPS) {
    return "Maximum " + std::to_string(MAX_GROUPS) + " groups";
  }
  return "";
}

void MacrosPanel::open_new_group(const std::string &assign_to) {
  name_dialog("New group", "", "Create", [this, assign_to](const std::string &name) -> std::string {
    const std::string error = validate_group_name(name, "");
    if (!error.empty()) {
      return error;
    }
    groups.push_back(name);
    if (!assign_to.empty()) {
      assignment[assign_to] = name;
    }
    save_groups();
    apply_filter();
    schedule_rebuild_pills();
    return "";
  });
}

void MacrosPanel::open_move_menu(const std::string &macro) {
  if (groups.empty()) {
    open_new_group(macro);
    return;
  }
  std::vector<std::string> options = groups;
  if ((int)groups.size() < MAX_GROUPS) {
    options.push_back("New group");
  }
  const std::vector<std::string> current = groups;
  powerui::choice_dialog("Move to group", options, [this, macro, current](int index) {
    if (index >= 0 && index < (int)current.size()) {
      assign_macro(macro, current[index]);
    } else {
      open_new_group(macro);
    }
  });
}

void MacrosPanel::assign_macro(const std::string &macro, const std::string &group) {
  if (group.empty()) {
    assignment.erase(macro);
  } else {
    assignment[macro] = group;
  }
  save_groups();
  apply_filter();
  schedule_rebuild_pills();
}

void MacrosPanel::open_group_options(const std::string &group) {
  powerui::choice_dialog("Group options", {"Rename", "Delete"}, [this, group](int index) {
    if (index == 0) {
      open_rename_group(group);
    } else if (index == 1) {
      confirm_delete_group(group);
    }
  });
}

void MacrosPanel::open_rename_group(const std::string &group) {
  name_dialog("Rename group", group, "Save", [this, group](const std::string &name) -> std::string {
    const std::string error = validate_group_name(name, group);
    if (!error.empty()) {
      return error;
    }
    std::replace(groups.begin(), groups.end(), group, name);
    for (auto &entry : assignment) {
      if (entry.second == group) {
        entry.second = name;
      }
    }
    if (selected == group) {
      selected = name;
    }
    save_groups();
    apply_filter();
    schedule_rebuild_pills();
    return "";
  });
}

void MacrosPanel::confirm_delete_group(const std::string &group) {
  powerui::confirm_dialog(("Delete " + group + "?").c_str(), "Its macros go back to All.", "Delete",
                          powerui::ActionKind::Destructive, [this, group]() {
    groups.erase(std::remove(groups.begin(), groups.end(), group), groups.end());
    for (auto entry = assignment.begin(); entry != assignment.end();) {
      entry = entry->second == group ? assignment.erase(entry) : std::next(entry);
    }
    if (selected == group) {
      selected.clear();
    }
    save_groups();
    apply_filter();
    schedule_rebuild_pills();
  });
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
