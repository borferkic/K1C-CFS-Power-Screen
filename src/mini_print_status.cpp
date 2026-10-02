#include "mini_print_status.h"
#include "powerui.h"
#include "spdlog/spdlog.h"

using namespace powerui;

LV_FONT_DECLARE(powerui_font_number_88);
LV_IMG_DECLARE(clock_img);
LV_IMG_DECLARE(pause_img);
LV_IMG_DECLARE(resume);
LV_IMG_DECLARE(cancel);
LV_IMG_DECLARE(ui_icon_exclude);

namespace {
// Content area of the card is 366 x 374 design px (408 x 416 card, 20 padding, 1 border).
constexpr int CONTENT_W = 366;

lv_obj_t *action_button(lv_obj_t *parent, int x, int y, int w, int h, uint32_t bg, uint32_t border,
                        lv_opa_t border_opa) {
  lv_obj_t *btn = plain(parent);
  lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_pos(btn, px(x), px(y));
  lv_obj_set_size(btn, px(w), px(h));
  lv_obj_set_style_radius(btn, px(10), 0);
  lv_obj_set_style_bg_color(btn, lv_color_hex(bg), 0);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(btn, 1, 0);
  lv_obj_set_style_border_color(btn, lv_color_hex(border), 0);
  lv_obj_set_style_border_opa(btn, border_opa, 0);
  lv_obj_set_style_bg_color(btn, lv_color_hex(COLOR_SECONDARY), LV_STATE_PRESSED);
  lv_obj_set_style_opa(btn, LV_OPA_40, LV_STATE_DISABLED);
  lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(btn, px(10), 0);
  return btn;
}
} // namespace

MiniPrintStatus::MiniPrintStatus(lv_obj_t *parent,
				 lv_event_cb_t cb,
				 void* user_data)
  : cont(card(parent, 12, 12, 408, 416))
  , title_label(NULL)
  , subtitle_label(NULL)
  , toggle(NULL)
  , state_badge(NULL)
  , number_label(NULL)
  , percent_label(NULL)
  , eta_label(NULL)
  , progress_bar(NULL)
  , elapsed_value(NULL)
  , layer_value(NULL)
  , speed_value(NULL)
  , pause_btn(NULL)
  , pause_icon(NULL)
  , pause_label(NULL)
  , stop_btn(NULL)
  , exclude_row(NULL)
  , status("n/a")
  , active(false)
{
  lv_obj_add_flag(cont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_pad_all(cont, px(20), 0);
  lv_obj_add_flag(cont, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(cont, cb, LV_EVENT_CLICKED, user_data);

  const lv_color_t fg = lv_color_hex(COLOR_FG);
  const lv_color_t muted = lv_color_hex(COLOR_MUTED);

  // Header: file name + subtitle on the left, state badge and view toggle on the right.
  title_label = label(cont, "No active print", &lv_font_montserrat_16, fg);
  lv_obj_set_width(title_label, px(190));
  lv_label_set_long_mode(title_label, LV_LABEL_LONG_DOT);
  lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 0, 0);

  subtitle_label = label(cont, "Ready", &lv_font_montserrat_14, muted);
  lv_obj_set_width(subtitle_label, px(190));
  lv_label_set_long_mode(subtitle_label, LV_LABEL_LONG_DOT);
  lv_obj_align(subtitle_label, LV_ALIGN_TOP_LEFT, 0, px(23));

  // "Exclude objects" chip: a 40 px tile (the size of the extruder icon tile) with the icon, and the text.
  exclude_row = plain(cont);
  lv_obj_add_flag(exclude_row, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(exclude_row, px(190), px(40));
  lv_obj_align(exclude_row, LV_ALIGN_TOP_LEFT, 0, px(44));
  lv_obj_t *exclude_tile = plain(exclude_row);
  lv_obj_set_size(exclude_tile, px(40), px(40));
  lv_obj_set_style_radius(exclude_tile, px(10), 0);
  lv_obj_set_style_bg_color(exclude_tile, lv_color_hex(COLOR_SECONDARY), 0);
  lv_obj_set_style_bg_opa(exclude_tile, LV_OPA_COVER, 0);
  lv_obj_t *exclude_icon = icon(exclude_tile, &ui_icon_exclude, 26, fg);
  lv_obj_center(exclude_icon);
  lv_obj_t *exclude_text = label(exclude_row, "Exclude objects", &lv_font_montserrat_14, muted);
  lv_obj_align(exclude_text, LV_ALIGN_LEFT_MID, px(52), 0);
  lv_obj_add_event_cb(exclude_row, &MiniPrintStatus::_handle_action, LV_EVENT_CLICKED, this);
  lv_obj_add_flag(exclude_row, LV_OBJ_FLAG_HIDDEN);

  toggle = view_toggle(cont, NULL, NULL);
  lv_obj_align(toggle, LV_ALIGN_TOP_RIGHT, 0, 0);

  state_badge = badge(cont, "Idle", muted);
  lv_obj_update_layout(toggle);
  lv_obj_align_to(state_badge, toggle, LV_ALIGN_OUT_LEFT_MID, -px(8), 0);

  // Progress: big number, percent sign, ETA and bar.
  number_label = label(cont, "0", &powerui_font_number_88, fg);
  lv_obj_align(number_label, LV_ALIGN_TOP_LEFT, 0, px(96));

  percent_label = label(cont, "%", &lv_font_montserrat_40, muted);

  lv_obj_t *eta_icon = icon(cont, &clock_img, 14, muted);
  lv_obj_align(eta_icon, LV_ALIGN_TOP_RIGHT, -px(34), px(114));
  lv_obj_t *eta_text = label(cont, "ETA", &lv_font_montserrat_14, muted);
  lv_obj_align(eta_text, LV_ALIGN_TOP_RIGHT, 0, px(112));

  eta_label = label(cont, "...", &lv_font_montserrat_24, fg);
  lv_obj_align(eta_label, LV_ALIGN_TOP_RIGHT, 0, px(132));

  progress_bar = lv_bar_create(cont);
  lv_bar_set_range(progress_bar, 0, 100);
  lv_obj_set_size(progress_bar, px(CONTENT_W), px(8));
  lv_obj_align(progress_bar, LV_ALIGN_TOP_LEFT, 0, px(178));
  lv_obj_set_style_radius(progress_bar, px(4), LV_PART_MAIN);
  lv_obj_set_style_bg_color(progress_bar, lv_color_hex(COLOR_SECONDARY), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(progress_bar, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(progress_bar, px(4), LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(progress_bar, lv_color_hex(COLOR_ACCENT), LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(progress_bar, LV_OPA_COVER, LV_PART_INDICATOR);

  // Statistics box: Elapsed / Layer / Speed.
  lv_obj_t *stats = plain(cont);
  lv_obj_set_size(stats, px(CONTENT_W), px(68));
  lv_obj_align(stats, LV_ALIGN_TOP_LEFT, 0, px(216));
  lv_obj_set_style_radius(stats, px(10), 0);
  lv_obj_set_style_border_width(stats, 1, 0);
  lv_obj_set_style_border_color(stats, lv_color_hex(COLOR_WHITE), 0);
  lv_obj_set_style_border_opa(stats, LV_OPA_10, 0);

  const char *stat_names[3] = {"Elapsed", "Layer", "Speed"};
  lv_obj_t **stat_values[3] = {&elapsed_value, &layer_value, &speed_value};
  const char *stat_defaults[3] = {"0s", "0 / 0", "100%"};
  for (int i = 0; i < 3; ++i) {
    const int cell_x = i * (CONTENT_W - 2) / 3;
    lv_obj_t *name_label = label(stats, stat_names[i], &lv_font_montserrat_12, muted);
    lv_obj_set_pos(name_label, px(cell_x + 15), px(12));
    *stat_values[i] = label(stats, stat_defaults[i], &lv_font_montserrat_18, fg);
    lv_obj_set_pos(*stat_values[i], px(cell_x + 15), px(31));
    if (i > 0) {
      lv_obj_t *sep = plain(stats);
      lv_obj_set_size(sep, 1, px(66));
      lv_obj_set_pos(sep, px(cell_x), 0);
      lv_obj_set_style_bg_color(sep, lv_color_hex(COLOR_WHITE), 0);
      lv_obj_set_style_bg_opa(sep, LV_OPA_10, 0);
    }
  }

  // Actions: Pause/Resume and Stop.
  const int btn_w = (CONTENT_W - 12) / 2;
  pause_btn = action_button(cont, 0, 314, btn_w, 60, COLOR_SECONDARY, COLOR_WHITE, LV_OPA_10);
  pause_icon = icon(pause_btn, &pause_img, 24, fg);
  pause_label = label(pause_btn, "Pause", &lv_font_montserrat_16, fg);
  lv_obj_add_event_cb(pause_btn, &MiniPrintStatus::_handle_action, LV_EVENT_CLICKED, this);

  stop_btn = action_button(cont, btn_w + 12, 314, btn_w, 60, COLOR_CARD, COLOR_DESTRUCTIVE, LV_OPA_40);
  icon(stop_btn, &cancel, 22, lv_color_hex(COLOR_DESTRUCTIVE));
  label(stop_btn, "Stop", &lv_font_montserrat_16, lv_color_hex(COLOR_DESTRUCTIVE));
  lv_obj_add_event_cb(stop_btn, &MiniPrintStatus::_handle_action, LV_EVENT_CLICKED, this);

  lv_obj_add_state(pause_btn, LV_STATE_DISABLED);
  lv_obj_add_state(stop_btn, LV_STATE_DISABLED);
  update_progress(0);
}

MiniPrintStatus::~MiniPrintStatus() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

void MiniPrintStatus::show() {
  if (!active) {
    active = true;
    if (state_callback) {
      state_callback(true);
    }
  }
}

void MiniPrintStatus::hide() {
  if (active) {
    active = false;
    if (state_callback) {
      state_callback(false);
    }
  }
}

bool MiniPrintStatus::is_active() const {
  return active;
}

lv_obj_t *MiniPrintStatus::get_container() {
  return cont;
}

lv_obj_t *MiniPrintStatus::get_toggle() {
  return toggle;
}

void MiniPrintStatus::set_state_callback(std::function<void(bool)> callback) {
  state_callback = callback;
}

void MiniPrintStatus::set_status_callback(std::function<void(const char *, lv_color_t)> callback) {
  status_callback = callback;
}

void MiniPrintStatus::set_actions(std::function<void()> pause, std::function<void()> resume, std::function<void()> stop) {
  pause_action = pause;
  resume_action = resume;
  stop_action = stop;
}

void MiniPrintStatus::set_exclude_action(std::function<void()> action) {
  exclude_action = action;
}

// Shows the "Exclude objects" chip only when the print has labeled objects.
void MiniPrintStatus::set_exclude_available(bool available) {
  if (available) {
    lv_obj_clear_flag(exclude_row, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(exclude_row, LV_OBJ_FLAG_HIDDEN);
  }
}

void MiniPrintStatus::handle_action(lv_event_t *event) {
  lv_obj_t *target = lv_event_get_target(event);
  if (target == exclude_row) {
    if (exclude_action) {
      exclude_action();
    }
    return;
  }
  if (target == pause_btn) {
    if (status == "paused" && resume_action) {
      resume_action();
    } else if (status != "paused" && pause_action) {
      pause_action();
    }
  } else if (target == stop_btn && stop_action) {
    stop_action();
  }
}

// Compact duration: "7h 06m", "5m 12s" or "42s".
static std::string compact_duration(uint32_t seconds) {
  const uint32_t h = seconds / 3600;
  const uint32_t m = (seconds % 3600) / 60;
  const uint32_t s = seconds % 60;
  if (h > 0) {
    return fmt::format("{}h {:02}m", h, m);
  }
  if (m > 0) {
    return fmt::format("{}m {:02}s", m, s);
  }
  return fmt::format("{}s", s);
}

void MiniPrintStatus::update_eta(uint32_t remaining_seconds) {
  lv_label_set_text(eta_label, compact_duration(remaining_seconds).c_str());
}

void MiniPrintStatus::refresh_subtitle() {
  std::string text = "Ready";
  if (status == "printing") {
    text = "Print in progress";
  } else if (status == "paused") {
    text = "Paused";
  } else if (status == "complete") {
    text = "Print complete";
  } else if (status == "cancelled") {
    text = "Print cancelled";
  } else if (status == "error") {
    text = "Print error";
  }
  if (!material.empty() && (status == "printing" || status == "paused")) {
    text += " - " + material;
  }
  lv_label_set_text(subtitle_label, text.c_str());
}

void MiniPrintStatus::update_status(std::string &status_str) {
  status = status_str;
  refresh_subtitle();

  const char *badge_text = "Idle";
  lv_color_t dot = lv_color_hex(COLOR_MUTED);
  if (status == "printing") {
    badge_text = "Printing";
    dot = lv_color_hex(COLOR_ACCENT);
  } else if (status == "paused") {
    badge_text = "Paused";
    dot = lv_color_hex(COLOR_WARNING);
  } else if (status == "complete") {
    badge_text = "Complete";
    dot = lv_color_hex(COLOR_ACCENT);
  } else if (status == "cancelled" || status == "error") {
    badge_text = status == "error" ? "Error" : "Cancelled";
    dot = lv_color_hex(COLOR_DESTRUCTIVE);
  }
  badge_set(state_badge, badge_text, dot);
  lv_obj_update_layout(state_badge);
  if (status_callback) {
    status_callback(badge_text, dot);
  }
  lv_obj_align_to(state_badge, toggle, LV_ALIGN_OUT_LEFT_MID, -px(8), 0);

  const bool running = status == "printing" || status == "paused";
  if (running) {
    lv_obj_clear_state(pause_btn, LV_STATE_DISABLED);
    lv_obj_clear_state(stop_btn, LV_STATE_DISABLED);
  } else {
    lv_obj_add_state(pause_btn, LV_STATE_DISABLED);
    lv_obj_add_state(stop_btn, LV_STATE_DISABLED);
  }

  const bool paused = status == "paused";
  lv_label_set_text(pause_label, paused ? "Resume" : "Pause");
  lv_img_set_src(lv_obj_get_child(pause_icon, 0), paused ? &resume : &pause_img);
}

void MiniPrintStatus::update_progress(int p) {
  lv_label_set_text(number_label, fmt::format("{}", p).c_str());
  lv_bar_set_value(progress_bar, p, LV_ANIM_OFF);
  lv_obj_update_layout(number_label);
  lv_obj_align_to(percent_label, number_label, LV_ALIGN_OUT_RIGHT_BOTTOM, px(4), px(-6));
}

void MiniPrintStatus::update_name(const std::string &name) {
  lv_label_set_text(title_label, name.c_str());
}

void MiniPrintStatus::update_material(const std::string &material_str) {
  material = material_str;
  refresh_subtitle();
}

void MiniPrintStatus::update_eta_unknown() {
  lv_label_set_text(eta_label, "...");
}

void MiniPrintStatus::update_elapsed(uint32_t elapsed_seconds) {
  lv_label_set_text(elapsed_value, compact_duration(elapsed_seconds).c_str());
}

void MiniPrintStatus::update_layer(int current, int total) {
  lv_label_set_text(layer_value, fmt::format("{} / {}", current, total).c_str());
}

void MiniPrintStatus::update_speed(int percent) {
  lv_label_set_text(speed_value, fmt::format("{}%", percent).c_str());
}

void MiniPrintStatus::reset() {
  update_progress(0);
  lv_label_set_text(eta_label, "...");
  lv_label_set_text(elapsed_value, "0s");
  lv_label_set_text(layer_value, "0 / 0");
  lv_label_set_text(title_label, "No active print");
  material.clear();
  std::string idle_status = "n/a";
  update_status(idle_status);
}
