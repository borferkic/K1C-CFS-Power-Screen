#include "sysinfo_panel.h"
#include "powerui.h"
#include "utils.h"
#include "config.h"
#include "state.h"
#include "spdlog/spdlog.h"
#include "subprocess.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <functional>
#include <memory>
#include <pthread.h>
#include <experimental/filesystem>
#include <iterator>
#include <map>
#include <vector>

namespace fs = std::experimental::filesystem;
namespace sp = subprocess;

LV_IMG_DECLARE(back);
LV_IMG_DECLARE(device);
LV_IMG_DECLARE(network_img);

#ifdef POWERSCREEN_VERSION
#define GS_VERSION POWERSCREEN_VERSION
#else
#define GS_VERSION "dev-snapshot"
#endif

namespace {
// Installed package version (.version next to the executable). A promoted
// official release reuses the nightly binary, so the compiled-in version
// is only a fallback.
std::string installed_version() {
  try {
    const fs::path file = fs::canonical("/proc/self/exe").parent_path() / ".version";
    std::ifstream in(file.string());
    if (in) {
      const json data = json::parse(in);
      if (data.contains("version") && data["version"].is_string()) {
        return data["version"].get<std::string>();
      }
    }
  } catch (const std::exception &error) {
    spdlog::debug("could not read package version: {}", error.what());
  }
  return GS_VERSION;
}

// Update channel selected in System: "nightly" (default) or "stable".
std::string update_channel() {
  auto &v = Config::get_instance()->get_json("/update_channel");
  if (v.is_string() && v.get<std::string>() == "stable") {
    return "stable";
  }
  return "nightly";
}

// "v0.34.5-nightly.20261001.085346Z" -> base "v0.34.5" and suffix "nightly.20261001.085346Z".
std::pair<std::string, std::string> split_version(const std::string &version) {
  const auto dash = version.find('-');
  if (dash == std::string::npos) {
    return {version, ""};
  }
  return {version.substr(0, dash), version.substr(dash + 1)};
}

constexpr uint32_t CARD_BORDER = 0x4ADE80;
constexpr uint32_t CREALITY_GREEN = 0x4ADE80;
constexpr uint32_t BUTTON_GREY = 0x262626;
constexpr uint32_t SCREEN_BACKGROUND = 0x0A0A0A;
constexpr uint32_t BACK_BUTTON_BACKGROUND = SCREEN_BACKGROUND;

lv_color_t screen_background_color() {
  return lv_color_hex(0x0A0A0A);
}

void style_screen_object(lv_obj_t *obj) {
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN);
}

void style_card(lv_obj_t *card) {
  style_screen_object(card);
  lv_obj_set_style_bg_color(card, lv_color_hex(0x171717), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_color(card, lv_color_white(), LV_PART_MAIN);
  lv_obj_set_style_border_opa(card, LV_OPA_10, LV_PART_MAIN);
  lv_obj_set_style_border_width(card, 1, LV_PART_MAIN);
  lv_obj_set_style_radius(card, 14, LV_PART_MAIN);
}

void style_row(lv_obj_t *row, lv_coord_t y) {
  style_screen_object(row);
  lv_obj_set_size(row, LV_PCT(100), 45);
  lv_obj_set_pos(row, 0, y);
}

// The row's text label (the dropdown or switch of the row is created first, so it is not always child 0).
lv_obj_t *find_row_label(lv_obj_t *row) {
  const uint32_t count = lv_obj_get_child_cnt(row);
  for (uint32_t i = 0; i < count; ++i) {
    lv_obj_t *child = lv_obj_get_child(row, i);
    if (lv_obj_check_type(child, &lv_label_class)) {
      return child;
    }
  }
  return NULL;
}

lv_obj_t *create_row_label(lv_obj_t *row, const char *text) {
  lv_obj_t *label = lv_label_create(row);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_set_style_translate_y(label, 5, LV_PART_MAIN);
  lv_obj_align(label, LV_ALIGN_LEFT_MID, 22, 0);
  return label;
}
}

namespace {
constexpr const char *BACKLIGHT_FILE = "/sys/class/backlight/backlight_pwm0/brightness";

// First "a.b.c.d" number found in a file's first 4 KB ("" when none).
std::string find_dotted_version(const std::string &path) {
  std::ifstream in(path);
  if (!in) {
    return "";
  }
  std::string text(4096, '\0');
  in.read(&text[0], text.size());
  text.resize(static_cast<size_t>(in.gcount()));
  for (size_t i = 0; i < text.size(); ++i) {
    size_t j = i;
    int groups = 0;
    while (j < text.size() && isdigit(static_cast<unsigned char>(text[j]))) {
      while (j < text.size() && isdigit(static_cast<unsigned char>(text[j]))) {
        ++j;
      }
      ++groups;
      if (groups < 4 && j + 1 < text.size() && text[j] == '.' && isdigit(static_cast<unsigned char>(text[j + 1]))) {
        ++j;
      } else {
        break;
      }
    }
    if (groups == 4) {
      return text.substr(i, j - i);
    }
    if (j > i) {
      i = j;
    }
  }
  return "";
}

// Creality firmware version of the K1C.
std::string k1c_firmware_version() {
  const char *candidates[] = {
    "/usr/data/creality/userdata/config/system_version.json",
    "/usr/data/creality/userdata/config/system_config.json",
    "/usr/data/creality/userdata/version",
    "/etc/version",
  };
  for (const char *path : candidates) {
    const std::string version = find_dotted_version(path);
    if (!version.empty()) {
      return version;
    }
  }
  return "unknown";
}

// Backlight brightness 0-100 (the K1C PWM backlight driver is configured with max_brightness=100).
void write_backlight(int percent) {
  std::ofstream out(BACKLIGHT_FILE);
  if (out) {
    out << std::max(5, std::min(100, percent));
  }
}

int read_backlight() {
  std::ifstream in(BACKLIGHT_FILE);
  int value = 0;
  if (in && (in >> value)) {
    return std::max(5, std::min(100, value));
  }
  return 100;
}
}

std::vector<std::string> SysInfoPanel::log_levels = {
  "trace",
  "debug",
  "info"
};

static std::map<int32_t, uint32_t> sleepsec_to_dd_idx = {
  {-1, 0}, // never
  {300, 1}, // 5 min
  {600, 2}, // 10 min
  {1800, 3}, // 30 min
  {3600, 4}, // 1 hour
  {18000, 5} // 5 hour
};

static std::map<std::string, int32_t> sleep_label_to_sec = {
  {"Never", -1}, // never
  {"5 Minutes", 300}, // 5 min
  {"10 Minutes", 600}, // 10 min
  {"30 Minutes", 1800}, // 30 min
  {"1 Hour", 3600}, // 1 hour
  {"5 Hours", 18000} // 5 hour
};

SysInfoPanel::SysInfoPanel()
  : cont(lv_obj_create(lv_scr_act()))
  , title_bar(lv_obj_create(cont))
  , title_label(lv_label_create(title_bar))
  , time_label(lv_label_create(title_bar))
  , clock_timer(NULL)
  , network_card(lv_obj_create(cont))
  , network_title_label(lv_label_create(network_card))
  , network_name_label(lv_label_create(network_card))
  , network_ip_label(lv_label_create(network_card))
  , printer_img(lv_img_create(cont))
  , controls_card(lv_obj_create(cont))
  , disp_sleep_cont(lv_obj_create(controls_card))
  , display_sleep_dd(lv_dropdown_create(disp_sleep_cont))
  , estop_toggle_cont(lv_obj_create(controls_card))
  , prompt_estop_toggle(lv_switch_create(estop_toggle_cont))
  , z_icon_toggle_cont(lv_obj_create(controls_card))
  , z_icon_toggle(lv_switch_create(z_icon_toggle_cont))
  , ll_cont(lv_obj_create(controls_card))
  , loglevel_dd(lv_dropdown_create(ll_cont))
  , loglevel(1)
  , channel_cont(lv_obj_create(controls_card))
  , channel_dd(lv_dropdown_create(channel_cont))
  , brand_label(lv_label_create(cont))
  , version_label(lv_label_create(cont))
  , update_button(lv_btn_create(cont))
  , update_button_label(lv_label_create(update_button))
  , update_status(lv_label_create(cont))
  , back_btn(cont, &back, "Back", &SysInfoPanel::_handle_callback, this)
  , tab_general_btn(NULL)
  , tab_updates_btn(NULL)
  , general_page(NULL)
  , updates_page(NULL)
  , updates_version_label(NULL)
  , update_overlay(NULL)
  , update_spinner(NULL)
  , update_title(NULL)
  , update_phase(NULL)
  , update_close_btn(NULL)
  , update_timer(NULL)
  , check_running(false)
  , check_done(false)
  , check_result(0)
  , update_running(false)
  , update_finished(false)
  , update_exit_code(0)
{
  Config *conf = Config::get_instance();

  {
    const auto &saved = conf->get_json("/display_brightness");
    if (saved.is_number_integer()) {
      write_backlight(saved.get<int>());
    }
  }

  style_screen_object(cont);
  lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(cont, lv_color_hex(SCREEN_BACKGROUND), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, LV_PART_MAIN);

  lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(title_bar, LV_PCT(100), 32);
  lv_obj_set_pos(title_bar, 0, 0);
  lv_obj_set_style_pad_all(title_bar, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_color(title_bar, lv_color_hex(0x171717), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(title_bar, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(title_bar, 0, LV_PART_MAIN);

  lv_label_set_text(title_label, "SYSTEM");
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
  clock_timer = lv_timer_create(&SysInfoPanel::_update_clock_cb, 1000, this);

  style_card(network_card);
  lv_obj_set_size(network_card, 264, 104);
  lv_obj_set_pos(network_card, 12, 47);

  lv_label_set_text(network_title_label, "Network");
  lv_obj_set_style_text_color(network_title_label, lv_color_hex(0xA1A1A1), LV_PART_MAIN);
  lv_obj_set_style_text_font(network_title_label, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_set_pos(network_title_label, 32, 18);

  lv_obj_set_width(network_name_label, 225);
  lv_obj_set_height(network_name_label, 24);
  lv_obj_set_style_text_color(network_name_label, lv_color_hex(0xFAFAFA), LV_PART_MAIN);
  lv_obj_set_style_text_font(network_name_label, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_label_set_long_mode(network_name_label, LV_LABEL_LONG_DOT);
  lv_obj_set_pos(network_name_label, 32, 42);

  lv_obj_set_width(network_ip_label, 225);
  lv_obj_set_height(network_ip_label, 24);
  lv_obj_set_style_text_color(network_ip_label, lv_color_hex(0xA1A1A1), LV_PART_MAIN);
  lv_obj_set_style_text_font(network_ip_label, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_set_pos(network_ip_label, 32, 66);

  lv_img_set_src(printer_img, &device);
  lv_obj_set_pos(printer_img, 0, 130);

  style_card(controls_card);
  lv_obj_set_size(controls_card, 410, 220);
  lv_obj_set_pos(controls_card, 314, 47);

  style_row(disp_sleep_cont, 13);
  create_row_label(disp_sleep_cont, "Display Sleep");
  lv_obj_set_size(display_sleep_dd, 130, 46);
  lv_obj_align(display_sleep_dd, LV_ALIGN_RIGHT_MID, -22, 0);
  lv_dropdown_set_options(display_sleep_dd,
                          "Never\n"
                          "5 Minutes\n"
                          "10 Minutes\n"
                          "30 Minutes\n"
                          "1 Hour\n"
                          "5 Hours");

  auto v = conf->get_json("/display_sleep_sec");
  if (!v.is_null()) {
    auto sleep_sec = v.template get<int32_t>();
    const auto &el = sleepsec_to_dd_idx.find(sleep_sec);
    if (el != sleepsec_to_dd_idx.end()) {
      lv_dropdown_set_selected(display_sleep_dd, el->second);
    }
  }
  lv_obj_add_event_cb(display_sleep_dd, &SysInfoPanel::_handle_callback,
                      LV_EVENT_VALUE_CHANGED, this);

  style_row(estop_toggle_cont, 58);
  create_row_label(estop_toggle_cont, "Prompt Emergency Stop");
  lv_obj_align(prompt_estop_toggle, LV_ALIGN_RIGHT_MID, -22, 0);

  v = conf->get_json("/prompt_emergency_stop");
  if (!v.is_null() && v.template get<bool>()) {
    lv_obj_add_state(prompt_estop_toggle, LV_STATE_CHECKED);
  } else if (v.is_null()) {
    lv_obj_add_state(prompt_estop_toggle, LV_STATE_CHECKED);
  } else {
    lv_obj_clear_state(prompt_estop_toggle, LV_STATE_CHECKED);
  }
  lv_obj_add_event_cb(prompt_estop_toggle, &SysInfoPanel::_handle_callback,
                      LV_EVENT_VALUE_CHANGED, this);

  style_row(z_icon_toggle_cont, 103);
  create_row_label(z_icon_toggle_cont, "Invert Z Icon");
  lv_obj_align(z_icon_toggle, LV_ALIGN_RIGHT_MID, -22, 0);

  v = conf->get_json("/invert_z_icon");
  if (!v.is_null() && v.template get<bool>()) {
    lv_obj_add_state(z_icon_toggle, LV_STATE_CHECKED);
  } else {
    lv_obj_clear_state(z_icon_toggle, LV_STATE_CHECKED);
  }
  lv_obj_add_event_cb(z_icon_toggle, &SysInfoPanel::_handle_callback,
                      LV_EVENT_VALUE_CHANGED, this);

  style_row(ll_cont, 148);
  create_row_label(ll_cont, "Log Level");
  lv_obj_set_size(loglevel_dd, 130, 46);
  lv_obj_align(loglevel_dd, LV_ALIGN_RIGHT_MID, -22, 0);
  lv_dropdown_set_options(loglevel_dd, fmt::format("{}", fmt::join(log_levels, "\n")).c_str());

  auto df = conf->get_json("/default_printer");
  json j_null;
  v = !df.empty() ? conf->get_json(conf->df() + "log_level") : j_null;
  if (!v.is_null()) {
    auto it = std::find(log_levels.begin(), log_levels.end(), v.template get<std::string>());
    if (it != std::end(log_levels)) {
      loglevel = std::distance(log_levels.begin(), it);
      lv_dropdown_set_selected(loglevel_dd, loglevel);
    }
  } else {
    lv_dropdown_set_selected(loglevel_dd, loglevel);
  }
  lv_obj_add_event_cb(loglevel_dd, &SysInfoPanel::_handle_callback,
                      LV_EVENT_VALUE_CHANGED, this);

  // Channel row: placed in the Updates tab (create_tabs).
  style_row(channel_cont, 0);
  create_row_label(channel_cont, "Update Channel");
  lv_obj_set_size(channel_dd, 130, 46);
  lv_obj_align(channel_dd, LV_ALIGN_RIGHT_MID, -22, 0);
  lv_dropdown_set_options(channel_dd, "Nightly\nStable");
  lv_dropdown_set_selected(channel_dd, update_channel() == "stable" ? 1 : 0);
  lv_obj_add_event_cb(channel_dd, &SysInfoPanel::_handle_callback,
                      LV_EVENT_VALUE_CHANGED, this);

  lv_label_set_text(brand_label, "PowerScreen by Boris SdK");
  lv_obj_set_style_text_color(brand_label, lv_color_hex(0xFAFAFA), LV_PART_MAIN);
  lv_obj_set_style_text_font(brand_label, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_set_pos(brand_label, 330, 292);

  lv_label_set_text(version_label, fmt::format("Version: {}", installed_version()).c_str());
  lv_obj_set_width(version_label, 410);
  lv_obj_set_style_text_color(version_label, lv_color_hex(0xA1A1A1), LV_PART_MAIN);
  lv_obj_set_style_text_font(version_label, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_label_set_long_mode(version_label, LV_LABEL_LONG_CLIP);
  lv_obj_set_pos(version_label, 330, 318);

  lv_obj_clear_flag(update_button, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(update_button, 300, 50);
  lv_obj_set_style_bg_color(update_button, lv_color_hex(0x16A34A),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(update_button, LV_OPA_COVER,
                          LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(update_button, lv_color_hex(0x15803D),
                            LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(update_button, LV_OPA_COVER,
                          LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_color(update_button, lv_color_hex(0x262626),
                            LV_PART_MAIN | LV_STATE_DISABLED);
  lv_obj_set_style_bg_opa(update_button, LV_OPA_COVER,
                          LV_PART_MAIN | LV_STATE_DISABLED);
  lv_obj_set_style_border_width(update_button, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(update_button, 12, LV_PART_MAIN);
  lv_obj_add_state(update_button, LV_STATE_DISABLED);

  lv_label_set_text(update_button_label, "UPDATE");
  lv_obj_set_width(update_button_label, LV_PCT(100));
  lv_obj_set_style_text_align(update_button_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_style_text_color(update_button_label, lv_color_white(), LV_PART_MAIN);
  lv_obj_set_style_text_font(update_button_label, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_align(update_button_label, LV_ALIGN_CENTER, 0, 0);
  lv_obj_add_event_cb(update_button, &SysInfoPanel::_handle_callback,
                      LV_EVENT_CLICKED, this);

  lv_label_set_text(update_status, "");
  lv_obj_set_style_text_color(update_status, lv_color_white(), LV_PART_MAIN);
  lv_obj_set_style_text_font(update_status, &lv_font_montserrat_16, LV_PART_MAIN);

  create_tabs();

  lv_obj_add_flag(back_btn.get_container(), LV_OBJ_FLAG_FLOATING);
  lv_obj_t *back_container = back_btn.get_container();
  lv_obj_set_style_pad_top(back_container, 6, LV_PART_MAIN);
  lv_obj_set_style_pad_bottom(back_container, 6, LV_PART_MAIN);
  lv_obj_set_style_bg_color(back_container, lv_color_hex(BACK_BUTTON_BACKGROUND),
                            LV_PART_MAIN);
  lv_obj_set_style_bg_opa(back_container, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(back_container, 12, LV_PART_MAIN);
  // LVGL only supports a uniform radius. This opaque right strip squares the
  // right corners while preserving the 12 px radius on the left side.
  lv_obj_set_style_clip_corner(back_container, false, LV_PART_MAIN);
  lv_obj_update_layout(back_container);
  lv_obj_t *back_right_edge = lv_obj_create(back_container);
  lv_obj_remove_style_all(back_right_edge);
  lv_obj_set_size(back_right_edge, 12, LV_PCT(100));
  lv_obj_align(back_right_edge, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_set_style_bg_color(back_right_edge,
                            lv_color_hex(BACK_BUTTON_BACKGROUND), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(back_right_edge, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_clear_flag(back_right_edge, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_align(back_btn.get_container(), LV_ALIGN_BOTTOM_RIGHT, -10, -14);
  lv_obj_move_foreground(back_btn.get_container());
  lv_obj_move_background(cont);

  create_update_overlay();
  update_timer = lv_timer_create(&SysInfoPanel::_poll_update_cb, 500, this);
  show_updated_notice();
}

SysInfoPanel::~SysInfoPanel() {
  if (clock_timer != NULL) {
    lv_timer_del(clock_timer);
    clock_timer = NULL;
  }

  if (update_timer != NULL) {
    lv_timer_del(update_timer);
    update_timer = NULL;
  }

  if (update_overlay != NULL) {
    lv_obj_del(update_overlay);
    update_overlay = NULL;
  }

  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

void SysInfoPanel::foreground() {
  lv_obj_move_foreground(cont);
  update_clock();
  refresh_network();
  show_tab(false);
  {
    std::lock_guard<std::mutex> guard(script_mutex);
    refresh_system_versions();
  }
  check_script_update();
  lv_obj_add_flag(back_btn.get_container(), LV_OBJ_FLAG_HIDDEN);  // Back lives in the title bar
  powerui::overlay_open("System", [this]() { lv_obj_move_background(cont); });
}

void SysInfoPanel::open_power_update() {
  lv_obj_move_foreground(cont);
  update_clock();
  lv_obj_add_flag(back_btn.get_container(), LV_OBJ_FLAG_HIDDEN);
  show_tab(false);
  power_update_direct = true;
  show_tab(true);
}

void SysInfoPanel::create_tabs() {
  // Full-screen, transparent, non-clickable pages: children keep their
  // absolute positions and touches still reach the title bar buttons.
  auto create_page = [this]() {
    lv_obj_t *page = lv_obj_create(cont);
    style_screen_object(page);
    lv_obj_set_size(page, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(page, 0, 0);
    lv_obj_clear_flag(page, LV_OBJ_FLAG_CLICKABLE);
    return page;
  };
  general_page = create_page();
  updates_page = create_page();

  lv_obj_set_parent(network_card, general_page);
  lv_obj_set_parent(printer_img, general_page);
  lv_obj_set_parent(controls_card, general_page);
  lv_obj_set_parent(brand_label, general_page);
  lv_obj_set_parent(version_label, general_page);

  // ---- System page (PowerUI): printer photo and system versions on the left; network and preferences on the right.
  using namespace powerui;
  lv_obj_add_flag(title_bar, LV_OBJ_FLAG_HIDDEN);  // the main title bar replaces the panel's own one

  // Left column: a compact printer card (photo on the left, model and size on the right) and a roomy
  // system information card with text as large as the other cards of the screen.
  lv_obj_t *printer_card = lv_obj_create(general_page);
  style_card(printer_card);
  lv_obj_set_size(printer_card, px(350), px(110));
  lv_obj_set_pos(printer_card, px(12), px(12));
  lv_obj_set_parent(printer_img, printer_card);
  lv_img_set_size_mode(printer_img, LV_IMG_SIZE_MODE_REAL);
  lv_img_set_zoom(printer_img, 72);
  lv_obj_align(printer_img, LV_ALIGN_LEFT_MID, px(14), 0);
  lv_obj_t *printer_name = label(printer_card, "Creality K1C", &lv_font_montserrat_20, lv_color_hex(COLOR_FG));
  lv_obj_align(printer_name, LV_ALIGN_LEFT_MID, px(124), -px(14));
  lv_obj_t *printer_info = label(printer_card, "CoreXY", &lv_font_montserrat_14, lv_color_hex(COLOR_MUTED));
  lv_obj_align(printer_info, LV_ALIGN_LEFT_MID, px(124), px(12));
  lv_obj_t *printer_size = label(printer_card, "220 x 220 x 250 mm", &lv_font_montserrat_14, lv_color_hex(COLOR_MUTED));
  lv_obj_align(printer_size, LV_ALIGN_LEFT_MID, px(124), px(34));

  lv_obj_t *info_card = lv_obj_create(general_page);
  style_card(info_card);
  lv_obj_set_size(info_card, px(350), px(294));
  lv_obj_set_pos(info_card, px(12), px(134));
  lv_obj_t *info_title = label(info_card, "System", &lv_font_montserrat_16, lv_color_hex(COLOR_FG));
  lv_obj_set_pos(info_title, px(20), px(14));
  const char *info_names[5] = {"PowerScreen", "CFS Power Script", "Klipper", "Moonraker", "K1C Firmware"};
  for (int i = 0; i < 5; ++i) {
    const int row_y = 46 + i * 49;
    lv_obj_t *line = plain(info_card);
    lv_obj_set_size(line, LV_PCT(100), 1);
    lv_obj_set_pos(line, 0, px(row_y));
    lv_obj_set_style_bg_color(line, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_10, 0);

    lv_obj_t *name = label(info_card, info_names[i], &lv_font_montserrat_16, lv_color_hex(COLOR_MUTED));
    lv_obj_set_pos(name, px(20), px(row_y + 14));
    lv_obj_t *value = label(info_card, "...", &lv_font_montserrat_16, lv_color_hex(COLOR_FG));
    lv_obj_set_width(value, px(160));
    lv_label_set_long_mode(value, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_set_pos(value, px(350 - 20 - 160), px(row_y + 14));
    sys_value_labels[i] = value;
  }
  lv_label_set_text(sys_value_labels[0], split_version(installed_version()).first.c_str());
  lv_label_set_text(sys_value_labels[4], k1c_firmware_version().c_str());

  // Right column: network card (opens Wi-Fi).
  lv_obj_set_size(network_card, px(350), px(110));
  lv_obj_set_pos(network_card, px(374), px(12));
  lv_obj_add_flag(network_card, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_color(network_card, lv_color_hex(COLOR_SECONDARY), LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_add_event_cb(network_card, &SysInfoPanel::_handle_callback, LV_EVENT_CLICKED, this);
  lv_obj_add_flag(network_title_label, LV_OBJ_FLAG_HIDDEN);
  lv_obj_t *network_tile = plain(network_card);
  lv_obj_set_size(network_tile, px(60), px(60));
  lv_obj_set_pos(network_tile, px(18), px(25));
  lv_obj_set_style_radius(network_tile, px(12), 0);
  lv_obj_set_style_bg_color(network_tile, lv_color_hex(COLOR_SECONDARY), 0);
  lv_obj_set_style_bg_opa(network_tile, LV_OPA_COVER, 0);
  lv_obj_t *network_icon = icon(network_tile, &network_img, 36, lv_color_hex(COLOR_ACCENT));
  lv_obj_center(network_icon);
  lv_obj_set_pos(network_name_label, px(92), px(22));
  lv_obj_set_width(network_name_label, px(220));
  lv_obj_set_height(network_name_label, px(24));
  lv_obj_set_style_text_font(network_name_label, &lv_font_montserrat_18, LV_PART_MAIN);
  lv_obj_set_pos(network_ip_label, px(92), px(50));
  lv_obj_set_width(network_ip_label, px(220));
  lv_obj_set_height(network_ip_label, LV_SIZE_CONTENT);
  lv_obj_set_style_text_font(network_ip_label, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_t *network_hint = label(network_card, "Tap to open Wi-Fi", &lv_font_montserrat_12, lv_color_hex(COLOR_MUTED));
  lv_obj_set_pos(network_hint, px(92), px(76));
  lv_obj_t *network_chevron = label(network_card, LV_SYMBOL_RIGHT, &lv_font_montserrat_16, lv_color_hex(COLOR_MUTED));
  lv_obj_align(network_chevron, LV_ALIGN_RIGHT_MID, -px(18), 0);

  // Right column: preferences card.
  lv_obj_set_size(controls_card, px(350), px(294));
  lv_obj_set_pos(controls_card, px(374), px(134));
  lv_obj_t *prefs_title = label(controls_card, "Preferences", &lv_font_montserrat_16, lv_color_hex(COLOR_FG));
  lv_obj_set_pos(prefs_title, px(20), px(14));

  lv_obj_add_flag(z_icon_toggle_cont, LV_OBJ_FLAG_HIDDEN);  // "Invert Z icon" is no longer offered
  struct PrefRow { lv_obj_t *row; lv_obj_t *control; int y; };
  PrefRow pref_rows[3] = {{disp_sleep_cont, display_sleep_dd, 46}, {estop_toggle_cont, prompt_estop_toggle, 182}, {ll_cont, loglevel_dd, 234}};
  for (const auto &pref : pref_rows) {
    lv_obj_set_size(pref.row, LV_PCT(100), px(52));
    lv_obj_set_pos(pref.row, 0, px(pref.y));
    lv_obj_t *row_label = find_row_label(pref.row);
    if (row_label != NULL) {
      lv_obj_set_style_text_font(row_label, &lv_font_montserrat_16, LV_PART_MAIN);
      lv_obj_set_style_translate_y(row_label, 0, LV_PART_MAIN);
      lv_obj_align(row_label, LV_ALIGN_LEFT_MID, px(20), 0);
    }
    lv_obj_align(pref.control, LV_ALIGN_RIGHT_MID, -px(16), 0);
  }
  lv_obj_set_size(display_sleep_dd, px(130), px(40));
  lv_obj_set_size(loglevel_dd, px(130), px(40));

  // Brightness: label, percentage and a slider (backlight 5-100 %).
  lv_obj_t *brightness_label = label(controls_card, "Brightness", &lv_font_montserrat_16, lv_color_hex(COLOR_FG));
  lv_obj_set_pos(brightness_label, px(20), px(98 + 14));
  brightness_value = label(controls_card, "100%", &lv_font_montserrat_14, lv_color_hex(COLOR_MUTED));
  lv_obj_align(brightness_value, LV_ALIGN_TOP_RIGHT, -px(20), px(98 + 16));
  brightness_slider = lv_slider_create(controls_card);
  lv_slider_set_range(brightness_slider, 5, 100);
  lv_obj_set_size(brightness_slider, px(310), px(16));
  lv_obj_set_pos(brightness_slider, px(20), px(98 + 52));
  lv_obj_set_style_bg_color(brightness_slider, lv_color_hex(COLOR_SECONDARY), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(brightness_slider, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(brightness_slider, lv_color_hex(COLOR_ACCENT), LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(brightness_slider, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(brightness_slider, lv_color_hex(COLOR_FG), LV_PART_KNOB);
  lv_obj_set_style_bg_opa(brightness_slider, LV_OPA_COVER, LV_PART_KNOB);
  lv_obj_set_style_pad_all(brightness_slider, px(6), LV_PART_KNOB);
  {
    const int current = read_backlight();
    lv_slider_set_value(brightness_slider, current, LV_ANIM_OFF);
    lv_label_set_text(brightness_value, fmt::format("{}%", current).c_str());
  }
  lv_obj_add_event_cb(brightness_slider, &SysInfoPanel::_handle_brightness, LV_EVENT_VALUE_CHANGED, this);
  lv_obj_add_event_cb(brightness_slider, &SysInfoPanel::_handle_brightness, LV_EVENT_RELEASED, this);

  for (int y : {46, 98, 182, 234}) {
    lv_obj_t *line = plain(controls_card);
    lv_obj_set_size(line, LV_PCT(100), 1);
    lv_obj_set_pos(line, 0, px(y));
    lv_obj_set_style_bg_color(line, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_10, 0);
  }

  lv_obj_add_flag(brand_label, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(version_label, LV_OBJ_FLAG_HIDDEN);

  // ---- Power Update page (PowerUI): K1C photo and credits on the left; PowerScreen and CFS Power Script cards on the right.
  auto make_update_button = [this](lv_obj_t *parent, const char *text) {
    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(button, px(376), px(48));
    lv_obj_set_style_bg_color(button, lv_color_hex(0x16A34A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x15803D), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x262626), LV_PART_MAIN | LV_STATE_DISABLED);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DISABLED);
    lv_obj_set_style_border_width(button, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(button, px(10), LV_PART_MAIN);
    lv_obj_t *button_label = label(button, text, &lv_font_montserrat_16, lv_color_white());
    lv_obj_center(button_label);
    return button;
  };
  // Title, big version and build suffix. Returns nothing; the labels are handed back through the out pointers.
  auto make_version_block = [this](lv_obj_t *parent, const char *title, lv_obj_t **version_out, lv_obj_t **suffix_out,
                                   const std::string &base, const std::string &suffix) {
    lv_obj_t *t = lv_label_create(parent);
    lv_label_set_text(t, title);
    lv_obj_set_style_text_color(t, lv_color_hex(COLOR_MUTED), LV_PART_MAIN);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_pos(t, px(20), px(14));
    lv_obj_t *v = lv_label_create(parent);
    lv_label_set_text(v, base.c_str());
    lv_label_set_long_mode(v, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(v, px(260));
    lv_obj_set_style_text_color(v, lv_color_hex(COLOR_FG), LV_PART_MAIN);
    lv_obj_set_style_text_font(v, &lv_font_montserrat_40, LV_PART_MAIN);
    lv_obj_set_pos(v, px(20), px(34));
    lv_obj_t *sfx = lv_label_create(parent);
    lv_label_set_text(sfx, suffix.c_str());
    lv_label_set_long_mode(sfx, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(sfx, px(360));
    lv_obj_set_style_text_color(sfx, lv_color_hex(COLOR_MUTED), LV_PART_MAIN);
    lv_obj_set_style_text_font(sfx, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_pos(sfx, px(22), px(88));
    *version_out = v;
    *suffix_out = sfx;
  };
  // Status text in the top-right corner of a card.
  auto make_status = [](lv_obj_t *parent, lv_obj_t *existing) {
    lv_obj_t *status = existing != NULL ? existing : lv_label_create(parent);
    if (existing != NULL) {
      lv_obj_set_parent(status, parent);
    }
    lv_label_set_text(status, "");
    lv_label_set_long_mode(status, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(status, px(250));
    lv_obj_set_style_text_align(status, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_set_style_text_color(status, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(status, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(status, LV_ALIGN_TOP_RIGHT, -px(18), px(14));
    return status;
  };

  // Left column: printer photo and credits.
  lv_obj_t *photo_card = lv_obj_create(updates_page);
  style_card(photo_card);
  lv_obj_set_size(photo_card, px(292), px(296));
  lv_obj_set_pos(photo_card, px(12), px(12));
  lv_obj_t *photo = lv_img_create(photo_card);
  lv_img_set_src(photo, &device);
  lv_img_set_zoom(photo, 150);
  lv_obj_align(photo, LV_ALIGN_CENTER, 0, -px(22));
  lv_obj_t *photo_name = label(photo_card, "Creality K1C", &lv_font_montserrat_18, lv_color_hex(COLOR_FG));
  lv_obj_align(photo_name, LV_ALIGN_BOTTOM_MID, 0, -px(32));
  lv_obj_t *photo_info = label(photo_card, "CoreXY  220 x 220 x 250 mm", &lv_font_montserrat_12, lv_color_hex(COLOR_MUTED));
  lv_obj_align(photo_info, LV_ALIGN_BOTTOM_MID, 0, -px(10));

  lv_obj_t *credits = lv_obj_create(updates_page);
  style_card(credits);
  lv_obj_set_size(credits, px(292), px(108));
  lv_obj_set_pos(credits, px(12), px(320));
  lv_obj_set_parent(brand_label, credits);
  lv_obj_clear_flag(brand_label, LV_OBJ_FLAG_HIDDEN);
  lv_label_set_text(brand_label, "Developed by Boris SdK");
  lv_obj_set_style_text_font(brand_label, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_align(brand_label, LV_ALIGN_CENTER, 0, -px(10));
  lv_obj_t *theme_label = label(credits, "PowerUI 2026", &lv_font_montserrat_12, lv_color_hex(COLOR_MUTED));
  lv_obj_align(theme_label, LV_ALIGN_CENTER, 0, px(14));

  // PowerScreen card.
  lv_obj_t *card = lv_obj_create(updates_page);
  style_card(card);
  lv_obj_set_size(card, px(408), px(226));
  lv_obj_set_pos(card, px(316), px(12));
  const auto parts = split_version(installed_version());
  make_version_block(card, "POWERSCREEN", &updates_version_label, &version_suffix_label, parts.first, parts.second);
  make_status(card, update_status);

  lv_obj_set_parent(channel_cont, card);
  lv_obj_set_size(channel_cont, px(376), px(46));
  lv_obj_set_pos(channel_cont, px(16), px(110));
  lv_obj_t *channel_label = find_row_label(channel_cont);
  if (channel_label != NULL) {
    lv_label_set_text(channel_label, "Update channel");
    lv_obj_set_style_text_font(channel_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_translate_y(channel_label, 0, LV_PART_MAIN);
    lv_obj_align(channel_label, LV_ALIGN_LEFT_MID, px(4), 0);
  }
  lv_obj_align(channel_dd, LV_ALIGN_RIGHT_MID, 0, 0);

  lv_obj_set_parent(update_button, card);
  lv_obj_set_size(update_button, px(376), px(48));
  lv_obj_align(update_button, LV_ALIGN_BOTTOM_MID, 0, -px(16));
  lv_obj_set_style_radius(update_button, px(10), LV_PART_MAIN);

  // CFS Power Script card.
  lv_obj_t *script_card = lv_obj_create(updates_page);
  style_card(script_card);
  lv_obj_set_size(script_card, px(408), px(178));
  lv_obj_set_pos(script_card, px(316), px(250));
  make_version_block(script_card, "CFS POWER SCRIPT", &script_version_label, &script_suffix_label, script_base, script_suffix);
  script_status_label = make_status(script_card, NULL);

  script_button = make_update_button(script_card, "UPDATE");
  lv_obj_align(script_button, LV_ALIGN_BOTTOM_MID, 0, -px(16));
  lv_obj_add_state(script_button, LV_STATE_DISABLED);
  lv_obj_add_event_cb(script_button, &SysInfoPanel::_handle_callback, LV_EVENT_CLICKED, this);

  // Tab buttons on the left of the title bar.
  auto create_tab_btn = [this](const char *text, lv_coord_t x) {
    lv_obj_t *btn = lv_btn_create(title_bar);
    lv_obj_set_size(btn, 120, 28);
    lv_obj_set_pos(btn, x, 2);
    lv_obj_set_style_radius(btn, 8, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x262626), LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x16A34A), LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_center(label);
    lv_obj_add_event_cb(btn, &SysInfoPanel::_handle_callback, LV_EVENT_CLICKED, this);
    return btn;
  };
  tab_general_btn = create_tab_btn("System", 8);
  tab_updates_btn = create_tab_btn("About", 134);

  lv_obj_move_foreground(title_bar);
  show_tab(false);
}

void SysInfoPanel::show_tab(bool updates) {
  if (updates) {
    if (lv_obj_has_flag(updates_page, LV_OBJ_FLAG_HIDDEN)) {
      powerui::overlay_open("Power Update", [this]() {
        show_tab(false);
        if (power_update_direct) {
          power_update_direct = false;
          lv_obj_move_background(cont);
        }
      });  // Back in the title bar returns to System (or closes the panel when opened from Settings)
    }
    lv_obj_add_flag(general_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(updates_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_state(tab_general_btn, LV_STATE_CHECKED);
    lv_obj_add_state(tab_updates_btn, LV_STATE_CHECKED);
    lv_label_set_text(updates_version_label, split_version(installed_version()).first.c_str());
    check_for_update();
    check_script_update();
  } else {
    lv_obj_clear_flag(general_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(updates_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_state(tab_general_btn, LV_STATE_CHECKED);
    lv_obj_clear_state(tab_updates_btn, LV_STATE_CHECKED);
  }
}

void SysInfoPanel::refresh_network() {
  const std::string wifi_interface = KUtils::get_wifi_interface();
  const std::string wifi_name = KUtils::get_wifi_network();
  const std::string wifi_ip = wifi_interface.empty()
                              ? ""
                              : KUtils::interface_ip(wifi_interface);

  lv_label_set_text(network_name_label,
                    wifi_name.empty() ? "Unavailable" : wifi_name.c_str());
  lv_label_set_text(network_ip_label,
                    wifi_ip.empty() ? "Unavailable" : wifi_ip.c_str());
}

namespace {
const char *UPDATE_STATUS_FILE = "/tmp/powerscreen-update.status";
const char *UPDATE_DONE_FILE = "/tmp/powerscreen-update.done";

fs::path update_script_path() {
  return fs::canonical("/proc/self/exe").parent_path() / "update.sh";
}

void *run_task(void *arg) {
  std::unique_ptr<std::function<void()>> task(static_cast<std::function<void()> *>(arg));
  (*task)();
  return NULL;
}

// Detached thread created with pthread_create. Do not use std::thread: in the
// static MIPS binary its weak pthread_create resolves to NULL and the process
// dies with SIGSEGV (epc = 0) when the thread is created.
void run_detached(std::function<void()> fn) {
  auto *task = new std::function<void()>(std::move(fn));
  pthread_t tid;
  if (pthread_create(&tid, NULL, &run_task, task) != 0) {
    spdlog::warn("Failed to start background task, running inline");
    run_task(task);
    return;
  }
  pthread_detach(tid);
}

std::string read_first_line(const char *path) {
  std::ifstream in(path);
  std::string line;
  std::getline(in, line);
  return line;
}

// Turn the "PHASE:detail" written by update.sh into on-screen text.
std::string phase_text(const std::string &status) {
  const auto sep = status.find(':');
  const std::string phase = status.substr(0, sep);
  const std::string detail = sep == std::string::npos ? "" : status.substr(sep + 1);

  if (phase == "CHECKING") return "Checking for the latest version...";
  if (phase == "DOWNLOADING") return "Downloading " + detail + "...";
  if (phase == "EXTRACTING") return "Installing " + detail + "...";
  if (phase == "RESTARTING") return "Restarting PowerScreen...";
  if (phase == "UP_TO_DATE") return "PowerScreen is already up to date (" + detail + ").";
  if (phase == "ERROR") return detail.empty() ? "Update failed." : detail;
  return "Preparing update...";
}
}

void SysInfoPanel::check_for_update() {
  lv_label_set_text(update_status, "Checking for updates...");
  lv_obj_set_style_text_color(update_status, lv_color_white(), LV_PART_MAIN);
  lv_obj_add_state(update_button, LV_STATE_DISABLED);

  // The GitHub query takes several seconds: run it on another thread so the
  // screen does not freeze. poll_update() applies the result.
  if (check_running.exchange(true)) {
    return;
  }

  run_detached([this]() {
    int result_code = 3;
    try {
      const fs::path script = update_script_path();
      if (fs::exists(script)) {
        const std::vector<std::string> command = {script.string(), "--check"};
        const auto output = sp::check_output(command);
        const std::string result(output.buf.data(), output.length);
        if (result.rfind("UPDATE_AVAILABLE:", 0) == 0) {
          result_code = 1;
        } else if (result.rfind("UP_TO_DATE:", 0) == 0) {
          result_code = 2;
        }
      } else {
        spdlog::warn("Failed to check for updates. Did not find update script.");
      }
    } catch (const std::exception &error) {
      spdlog::warn("Failed to check for PowerScreen updates: {}", error.what());
    }
    check_result = result_code;
    check_done = true;
    check_running = false;
  });
}

bool SysInfoPanel::is_printing() {
  auto &pstate = State::get_instance()->get_data("/printer_state/print_stats/state"_json_pointer);
  if (!pstate.is_string()) {
    return false;
  }
  const std::string state = pstate.template get<std::string>();
  return state == "printing" || state == "paused";
}

void SysInfoPanel::start_update() {
  if (update_running) {
    return;
  }

  if (is_printing()) {
    show_update_overlay("Update not available",
                        "PowerScreen cannot be updated while a print is in progress.", false);
    return;
  }

  fs::path script;
  try {
    script = update_script_path();
  } catch (const std::exception &error) {
    spdlog::warn("Failed to update PowerScreen: {}", error.what());
  }
  if (script.empty() || !fs::exists(script)) {
    spdlog::warn("Failed to update PowerScreen. Did not find update script.");
    show_update_overlay("Update failed", "Update script not found.", false);
    return;
  }

  std::remove(UPDATE_STATUS_FILE);
  update_finished = false;
  update_running = true;
  update_step = -1;
  restart_since = 0;
  show_update_overlay("Updating PowerScreen", "Preparing update...", true);

  const std::string script_str = script.string();
  run_detached([this, script_str]() {
    int code = -1;
    try {
      code = sp::call(script_str);
    } catch (const std::exception &error) {
      spdlog::warn("Failed to update PowerScreen: {}", error.what());
    }
    update_exit_code = code;
    update_finished = true;
  });
}

void SysInfoPanel::poll_update() {
  if (script_dirty.exchange(false)) {
    apply_script_state();
  }

  if (check_done.exchange(false)) {
    switch (check_result.load()) {
    case 1:
      lv_label_set_text(update_status, "NEW UPDATE AVAILABLE!");
      lv_obj_set_style_text_color(update_status, lv_color_hex(CREALITY_GREEN), LV_PART_MAIN);
      lv_obj_clear_state(update_button, LV_STATE_DISABLED);
      break;
    case 2:
      lv_label_set_text(update_status, "PowerScreen is up to date.");
      lv_obj_set_style_text_color(update_status, lv_color_white(), LV_PART_MAIN);
      break;
    default:
      lv_label_set_text(update_status, "Could not check for updates.");
      lv_obj_set_style_text_color(update_status, lv_color_hex(0xF44336), LV_PART_MAIN);
      break;
    }
  }

  if (!update_running) {
    return;
  }

  const std::string status = read_first_line(UPDATE_STATUS_FILE);
  if (!status.empty()) {
    const auto sep = status.find(':');
    const std::string phase = status.substr(0, sep);
    const std::string detail = sep == std::string::npos ? "" : status.substr(sep + 1);
    int step = -1;
    if (phase == "CHECKING" || phase == "DOWNLOADING") {
      step = 0;
    } else if (phase == "EXTRACTING") {
      step = 1;
    } else if (phase == "RESTARTING") {
      step = 2;
      if (restart_since == 0) {
        restart_since = std::time(nullptr);
      }
    }
    if (!detail.empty() && step >= 0) {
      lv_label_set_text(update_subtitle, fmt::format("{} -> {}", split_version(installed_version()).first, split_version(detail).first).c_str());
    } else if (step >= 0) {
      lv_label_set_text(update_subtitle, "Checking for the latest version...");
    }
    if (step >= 0 && step != update_step) {
      set_update_step(step);
    }
  }

  // Restarting: the script stops PowerScreen by itself. If we are still alive well after that, the restart failed.
  if (restart_since != 0 && std::time(nullptr) - restart_since > 30) {
    update_running = false;
    show_update_overlay("Restart needed", "Restart PowerScreen to apply the new version.", false);
    return;
  }

  if (!update_finished) {
    return;
  }

  if (status.rfind("RESTARTING", 0) == 0 && update_exit_code == 0) {
    return;  // keep the Restarting step on screen until the restart (or the safety net above)
  }

  // The script ended without restarting PowerScreen: error or no changes.
  update_running = false;
  restart_since = 0;
  if (status.rfind("UP_TO_DATE", 0) == 0) {
    show_update_overlay("No update needed", phase_text(status), false);
  } else if (status.rfind("ERROR", 0) == 0) {
    show_update_overlay("Update failed", phase_text(status), false);
  } else if (update_exit_code != 0) {
    show_update_overlay("Update failed", "The update script did not finish.", false);
  } else {
    show_update_overlay("Restart needed", "Restart PowerScreen to apply the new version.", false);
  }
}

namespace {
const char *POWER_SCRIPT_NAME = "CFS-Power-Script";
}

void SysInfoPanel::check_script_update() {
  if (ws_client == NULL) {
    return;
  }
  lv_label_set_text(script_status_label, "Checking for updates...");
  lv_obj_add_state(script_button, LV_STATE_DISABLED);

  ws_client->send_jsonrpc("machine.update.status", json{{"refresh", false}}, [this](json &j) {
    std::lock_guard<std::mutex> guard(script_mutex);
    if (j.contains("result") && j["result"].contains("version_info")) {
      const json &versions = j["result"]["version_info"];
      auto read_version = [&versions](const char *name, std::string &out) {
        if (versions.contains(name) && versions[name].is_object()) {
          out = versions[name].value("version", std::string("unknown"));
        }
      };
      read_version("klipper", klipper_version);
      read_version("moonraker", moonraker_version);
    }
    json info;
    if (j.contains("result") && j["result"].contains("version_info") && j["result"]["version_info"].contains(POWER_SCRIPT_NAME)) {
      info = j["result"]["version_info"][POWER_SCRIPT_NAME];
    }
    if (info.is_object()) {
      const std::string version = info.value("version", std::string());
      const std::string remote = info.value("remote_version", std::string());
      const std::string full = info.value("full_version_string", version);
      const bool behind = info.contains("commits_behind") && info["commits_behind"].is_array() && !info["commits_behind"].empty();
      const auto parts = split_version(version);
      script_base = parts.first.empty() ? "unknown" : parts.first;
      script_suffix = full;
      script_state = (behind || (!remote.empty() && remote != version)) ? 2 : 1;
      script_message = script_state == 2 ? "New version " + remote + " available." : "Power Script is up to date.";
    } else {
      script_state = 3;
      script_message = "Not in the update manager.";
    }
    script_dirty = true;
  });
}

void SysInfoPanel::start_script_update() {
  if (ws_client == NULL) {
    return;
  }
  if (is_printing()) {
    std::lock_guard<std::mutex> guard(script_mutex);
    script_state = 3;
    script_message = "It cannot be updated while a print is in progress.";
    script_dirty = true;
    return;
  }
  {
    std::lock_guard<std::mutex> guard(script_mutex);
    script_state = 4;
    script_message = "Updating the Power Script...";
    script_dirty = true;
  }
  ws_client->send_jsonrpc("machine.update.upgrade", json{{"name", POWER_SCRIPT_NAME}}, [this](json &j) {
    std::lock_guard<std::mutex> guard(script_mutex);
    if (j.contains("error")) {
      script_state = 3;
      script_message = j["error"].is_object() ? j["error"].value("message", std::string("Update failed.")) : "Update failed.";
    } else {
      script_state = 5;
      script_message = "Power Script updated.";
    }
    script_dirty = true;
  });
}

// Runs on the LVGL thread (from poll_update) to show the last result of the Power Script check or update.
void SysInfoPanel::apply_script_state() {
  std::lock_guard<std::mutex> guard(script_mutex);
  refresh_system_versions();
  lv_label_set_text(script_version_label, script_base.c_str());
  lv_label_set_text(script_suffix_label, script_suffix.c_str());
  lv_label_set_text(script_status_label, script_message.c_str());
  lv_obj_set_style_text_color(script_status_label,
                              lv_color_hex(script_state == 2 || script_state == 5 ? 0x4ADE80 : (script_state == 3 ? 0xFF6467 : 0xFAFAFA)),
                              LV_PART_MAIN);
  if (script_state == 2) {
    lv_obj_clear_state(script_button, LV_STATE_DISABLED);
  } else {
    lv_obj_add_state(script_button, LV_STATE_DISABLED);
  }
}

void SysInfoPanel::create_update_overlay() {
  using namespace powerui;
  update_overlay = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(update_overlay);
  lv_obj_set_size(update_overlay, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(update_overlay, lv_color_black(), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(update_overlay, LV_OPA_70, LV_PART_MAIN);
  // Swallow all touches to lock the UI while updating.
  lv_obj_add_flag(update_overlay, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(update_overlay, LV_OBJ_FLAG_SCROLLABLE);

  update_card = lv_obj_create(update_overlay);
  lv_obj_set_size(update_card, px(500), px(316));
  lv_obj_center(update_card);
  lv_obj_clear_flag(update_card, LV_OBJ_FLAG_SCROLLABLE);
  style_card(update_card);
  lv_obj_set_style_border_opa(update_card, LV_OPA_30, LV_PART_MAIN);
  lv_obj_set_style_radius(update_card, px(14), LV_PART_MAIN);

  update_icon_tile = plain(update_card);
  lv_obj_set_size(update_icon_tile, px(48), px(48));
  lv_obj_set_pos(update_icon_tile, px(28), px(24));
  lv_obj_set_style_radius(update_icon_tile, px(12), 0);
  lv_obj_set_style_bg_color(update_icon_tile, lv_color_hex(COLOR_SECONDARY), 0);
  lv_obj_set_style_bg_opa(update_icon_tile, LV_OPA_COVER, 0);
  update_icon_label = label(update_icon_tile, LV_SYMBOL_REFRESH, &lv_font_montserrat_24, lv_color_hex(COLOR_ACCENT));
  lv_obj_center(update_icon_label);

  update_title = label(update_card, "Updating PowerScreen", &lv_font_montserrat_20, lv_color_hex(COLOR_FG));
  lv_obj_set_pos(update_title, px(90), px(28));
  update_subtitle = label(update_card, "", &lv_font_montserrat_14, lv_color_hex(COLOR_MUTED));
  lv_obj_set_pos(update_subtitle, px(90), px(56));

  update_rule = plain(update_card);
  lv_obj_set_size(update_rule, px(444), 1);
  lv_obj_set_pos(update_rule, px(28), px(94));
  lv_obj_set_style_bg_color(update_rule, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(update_rule, LV_OPA_10, 0);

  const char *step_names[3] = {"Download", "Install files", "Restart PowerScreen"};
  for (int i = 0; i < 3; ++i) {
    update_marks[i] = plain(update_card);
    lv_obj_set_size(update_marks[i], px(24), px(24));
    lv_obj_set_pos(update_marks[i], px(34), px(110 + i * 38));
    update_step_labels[i] = label(update_card, step_names[i], &lv_font_montserrat_16, lv_color_hex(COLOR_MUTED));
    lv_obj_set_pos(update_step_labels[i], px(72), px(112 + i * 38));
  }

  update_bar = lv_bar_create(update_card);
  lv_obj_set_size(update_bar, px(444), px(10));
  lv_obj_set_pos(update_bar, px(28), px(240));
  lv_bar_set_range(update_bar, 0, 100);
  lv_obj_set_style_bg_color(update_bar, lv_color_hex(COLOR_SECONDARY), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(update_bar, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(update_bar, lv_color_hex(COLOR_ACCENT), LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(update_bar, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_radius(update_bar, LV_RADIUS_CIRCLE, LV_PART_MAIN);
  lv_obj_set_style_radius(update_bar, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);

  update_hint = label(update_card, "", &lv_font_montserrat_12, lv_color_hex(COLOR_MUTED));
  lv_obj_set_pos(update_hint, px(28), px(262));

  // Message for the end states (error, nothing to update, restart needed).
  update_phase = label(update_card, "", &lv_font_montserrat_14, lv_color_hex(COLOR_MUTED));
  lv_label_set_long_mode(update_phase, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(update_phase, px(420));
  lv_obj_set_style_text_align(update_phase, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_align(update_phase, LV_ALIGN_TOP_MID, 0, px(128));

  update_close_btn = lv_btn_create(update_card);
  lv_obj_set_size(update_close_btn, px(160), px(40));
  lv_obj_align(update_close_btn, LV_ALIGN_BOTTOM_MID, 0, -px(28));
  lv_obj_set_style_bg_color(update_close_btn, lv_color_hex(0x16A34A), LV_PART_MAIN);
  lv_obj_set_style_bg_color(update_close_btn, lv_color_hex(0x15803D), LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_border_width(update_close_btn, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(update_close_btn, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(update_close_btn, px(10), LV_PART_MAIN);
  lv_obj_t *close_label = label(update_close_btn, "Close", &lv_font_montserrat_16, lv_color_white());
  lv_obj_center(close_label);
  lv_obj_add_event_cb(update_close_btn, &SysInfoPanel::_handle_callback, LV_EVENT_CLICKED, this);

  lv_obj_add_flag(update_overlay, LV_OBJ_FLAG_HIDDEN);
}

// Progress steps of the update: 0 download, 1 install, 2 restart. The bar follows the phase (not real bytes).
void SysInfoPanel::set_update_step(int step) {
  using namespace powerui;
  update_step = step;
  static const int bar_values[3] = {25, 60, 100};
  lv_bar_set_value(update_bar, bar_values[std::max(0, std::min(2, step))], LV_ANIM_ON);
  for (int i = 0; i < 3; ++i) {
    lv_obj_clean(update_marks[i]);
    if (i < step) {
      lv_obj_t *mark = label(update_marks[i], LV_SYMBOL_OK, &lv_font_montserrat_16, lv_color_hex(COLOR_ACCENT));
      lv_obj_center(mark);
      lv_obj_set_style_text_color(update_step_labels[i], lv_color_hex(COLOR_ACCENT), LV_PART_MAIN);
    } else if (i == step) {
      lv_obj_t *spinner = lv_spinner_create(update_marks[i], 1000, 60);
      lv_obj_set_size(spinner, px(20), px(20));
      lv_obj_center(spinner);
      lv_obj_set_style_arc_width(spinner, 3, LV_PART_MAIN);
      lv_obj_set_style_arc_width(spinner, 3, LV_PART_INDICATOR);
      lv_obj_set_style_arc_color(spinner, lv_color_hex(COLOR_SECONDARY), LV_PART_MAIN);
      lv_obj_set_style_arc_color(spinner, lv_color_hex(COLOR_ACCENT), LV_PART_INDICATOR);
      lv_obj_set_style_text_color(update_step_labels[i], lv_color_hex(COLOR_FG), LV_PART_MAIN);
    } else {
      lv_obj_t *dot = plain(update_marks[i]);
      lv_obj_set_size(dot, px(8), px(8));
      lv_obj_center(dot);
      lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
      lv_obj_set_style_bg_color(dot, lv_color_hex(0x404040), 0);
      lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
      lv_obj_set_style_text_color(update_step_labels[i], lv_color_hex(COLOR_MUTED), LV_PART_MAIN);
    }
  }
  lv_label_set_text(update_hint, step >= 2 ? "The screen will be back in a few seconds." : "Do not turn off the printer. The screen restarts by itself.");
}

void SysInfoPanel::show_update_overlay(const std::string &title, const std::string &phase, bool busy) {
  using namespace powerui;
  lv_label_set_text(update_title, title.c_str());
  const bool failure = title == "Update failed" || title == "Update not available";
  lv_label_set_text(update_icon_label, busy ? LV_SYMBOL_REFRESH : (failure ? LV_SYMBOL_CLOSE : LV_SYMBOL_OK));
  lv_obj_set_style_text_color(update_icon_label, lv_color_hex(failure ? COLOR_DESTRUCTIVE : COLOR_ACCENT), LV_PART_MAIN);
  lv_obj_set_style_bg_color(update_icon_tile, failure ? lv_color_hex(0x2A1517) : lv_color_hex(COLOR_SECONDARY), 0);

  for (int i = 0; i < 3; ++i) {
    for (lv_obj_t *obj : {update_marks[i], update_step_labels[i]}) {
      if (busy) {
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
      }
    }
  }
  for (lv_obj_t *obj : {update_bar, update_hint}) {
    if (busy) {
      lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
  }
  if (busy) {
    lv_label_set_text(update_subtitle, phase.c_str());
    lv_obj_add_flag(update_phase, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(update_close_btn, LV_OBJ_FLAG_HIDDEN);
    if (update_step < 0) {
      set_update_step(0);
    }
  } else {
    update_step = -1;
    restart_since = 0;
    lv_label_set_text(update_subtitle, "");
    lv_label_set_text(update_phase, phase.c_str());
    lv_obj_clear_flag(update_phase, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(update_close_btn, LV_OBJ_FLAG_HIDDEN);
  }
  lv_obj_clear_flag(update_overlay, LV_OBJ_FLAG_HIDDEN);
}

void SysInfoPanel::show_updated_notice() {
  // update.sh leaves this file before restarting; the new version shows
  // it once on startup.
  const std::string version = read_first_line(UPDATE_DONE_FILE);
  if (version.empty()) {
    return;
  }
  std::remove(UPDATE_DONE_FILE);
  std::remove(UPDATE_STATUS_FILE);

  using namespace powerui;
  lv_obj_t *notice = lv_obj_create(lv_layer_top());
  lv_obj_set_size(notice, px(400), px(64));
  lv_obj_align(notice, LV_ALIGN_TOP_MID, 0, px(46));
  lv_obj_clear_flag(notice, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(notice, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_color(notice, lv_color_hex(0x0F1F15), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(notice, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(notice, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(notice, lv_color_hex(COLOR_ACCENT), LV_PART_MAIN);
  lv_obj_set_style_border_opa(notice, LV_OPA_50, LV_PART_MAIN);
  lv_obj_set_style_radius(notice, px(14), LV_PART_MAIN);
  lv_obj_set_style_pad_all(notice, 0, LV_PART_MAIN);

  lv_obj_t *mark = plain(notice);
  lv_obj_set_size(mark, px(32), px(32));
  lv_obj_align(mark, LV_ALIGN_LEFT_MID, px(30), 0);
  lv_obj_set_style_radius(mark, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(mark, lv_color_hex(0x16A34A), 0);
  lv_obj_set_style_bg_opa(mark, LV_OPA_COVER, 0);
  lv_obj_t *mark_label = label(mark, LV_SYMBOL_OK, &lv_font_montserrat_16, lv_color_white());
  lv_obj_center(mark_label);

  lv_obj_t *text = label(notice, "PowerScreen UPDATED!", &lv_font_montserrat_20, lv_color_hex(COLOR_FG));
  lv_obj_align(text, LV_ALIGN_LEFT_MID, px(76), 0);

  lv_obj_del_delayed(notice, 6000);
}

void SysInfoPanel::update_clock() {
  const std::time_t now = std::time(nullptr);
  const std::tm local_time = *std::localtime(&now);
  char time_text[6] = {};
  std::strftime(time_text, sizeof(time_text), "%H:%M", &local_time);
  lv_label_set_text(time_label, time_text);
}

void SysInfoPanel::handle_callback(lv_event_t *e)
{
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    lv_obj_t *btn = lv_event_get_current_target(e);

    if (btn == network_card) {
      if (wifi_callback) {
        wifi_callback();
      }
    } else if (btn == tab_general_btn) {
      show_tab(false);
    } else if (btn == tab_updates_btn || btn == about_row) {
      show_tab(true);
    } else if (btn == back_btn.get_container()) {
      if (!lv_obj_has_flag(updates_page, LV_OBJ_FLAG_HIDDEN)) {
        show_tab(false);  // About -> System
      } else {
        lv_obj_move_background(cont);
      }
    } else if (btn == update_button) {
      if (lv_obj_has_state(update_button, LV_STATE_DISABLED)) {
        return;
      }
      spdlog::trace("update powerscreen pressed from system info");
      start_update();
    } else if (btn == script_button) {
      if (!lv_obj_has_state(script_button, LV_STATE_DISABLED)) {
        start_script_update();
      }
    } else if (btn == update_close_btn) {
      lv_obj_add_flag(update_overlay, LV_OBJ_FLAG_HIDDEN);
    }
  } else if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
    lv_obj_t *obj = lv_event_get_target(e);
    Config *conf = Config::get_instance();

    if (obj == loglevel_dd) {
      auto idx = lv_dropdown_get_selected(loglevel_dd);
      if (idx != loglevel && idx < log_levels.size()) {
        loglevel = idx;
        auto ll = spdlog::level::from_str(log_levels[loglevel]);

        spdlog::set_level(ll);
        spdlog::flush_on(ll);
        spdlog::debug("setting log_level to {}", log_levels[loglevel]);
        conf->set<std::string>(conf->df() + "log_level", log_levels[loglevel]);
        conf->save();
      }
    } else if (obj == channel_dd) {
      const std::string channel = lv_dropdown_get_selected(channel_dd) == 1 ? "stable" : "nightly";
      if (channel != update_channel()) {
        spdlog::debug("setting update_channel to {}", channel);
        conf->set<std::string>("/update_channel", channel);
        conf->save();
        // Align the Moonraker/Fluidd Update Manager channel.
        run_detached([channel]() {
          try {
            sp::call(std::vector<std::string>{update_script_path().string(), "--set-channel", channel});
          } catch (const std::exception &error) {
            spdlog::warn("Failed to set update channel: {}", error.what());
          }
        });
        check_for_update();
      }
    } else if (obj == prompt_estop_toggle) {
      bool should_prompt = lv_obj_has_state(prompt_estop_toggle, LV_STATE_CHECKED);
      conf->set<bool>("/prompt_emergency_stop", should_prompt);
      conf->save();
    } else if (obj == display_sleep_dd) {
      char buf[64];
      lv_dropdown_get_selected_str(display_sleep_dd, buf, sizeof(buf));
      const auto el = sleep_label_to_sec.find(std::string(buf));
      if (el != sleep_label_to_sec.end()) {
        conf->set<int32_t>("/display_sleep_sec", el->second);
        conf->save();
      }
    } else if (obj == z_icon_toggle) {
      bool inverted = lv_obj_has_state(z_icon_toggle, LV_STATE_CHECKED);
      conf->set<bool>("/invert_z_icon", inverted);
      conf->save();
    }
  }
}

void SysInfoPanel::refresh_system_versions() {
  // Caller holds script_mutex.
  if (sys_value_labels[1] != NULL) {
    lv_label_set_text(sys_value_labels[1], script_base.c_str());
    std::string klipper = klipper_version;
    if (klipper == "..." || klipper == "unknown") {
      // The K1C update manager has no data for Klipper: use the version reported by printer.info.
      const auto &software_version = State::get_instance()->get_data("/printer_info/software_version"_json_pointer);
      if (software_version.is_string() && !software_version.template get<std::string>().empty()) {
        klipper = software_version.template get<std::string>();
      }
    }
    lv_label_set_text(sys_value_labels[2], klipper.c_str());
    lv_label_set_text(sys_value_labels[3], moonraker_version.c_str());
  }
}

void SysInfoPanel::handle_brightness(lv_event_t *event) {
  const int value = lv_slider_get_value(brightness_slider);
  if (lv_event_get_code(event) == LV_EVENT_VALUE_CHANGED) {
    lv_label_set_text(brightness_value, fmt::format("{}%", value).c_str());
    write_backlight(value);
  } else if (lv_event_get_code(event) == LV_EVENT_RELEASED) {
    Config *conf = Config::get_instance();
    conf->set<int>("/display_brightness", value);
    conf->save();
  }
}
