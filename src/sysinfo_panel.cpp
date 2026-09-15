#include "sysinfo_panel.h"
#include "utils.h"
#include "config.h"
#include "spdlog/spdlog.h"
#include "subprocess.hpp"

#include <algorithm>
#include <ctime>
#include <experimental/filesystem>
#include <iterator>
#include <map>
#include <vector>

namespace fs = std::experimental::filesystem;
namespace sp = subprocess;

LV_IMG_DECLARE(back);
LV_IMG_DECLARE(device);

#ifdef POWERSCREEN_VERSION
#define GS_VERSION POWERSCREEN_VERSION
#else
#define GS_VERSION "dev-snapshot"
#endif

namespace {
constexpr uint32_t CARD_BORDER = 0x4CAF50;
constexpr uint32_t CREALITY_GREEN = 0x4CAF50;
constexpr uint32_t BUTTON_GREY = 0x555555;

lv_color_t screen_background_color() {
  return lv_palette_darken(LV_PALETTE_GREY, 4);
}

void style_screen_object(lv_obj_t *obj) {
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN);
}

void style_card(lv_obj_t *card) {
  style_screen_object(card);
  lv_obj_set_style_bg_color(card, screen_background_color(), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_color(card, lv_color_hex(CARD_BORDER), LV_PART_MAIN);
  lv_obj_set_style_border_width(card, 1, LV_PART_MAIN);
  lv_obj_set_style_radius(card, 12, LV_PART_MAIN);
}

void style_row(lv_obj_t *row, lv_coord_t y) {
  style_screen_object(row);
  lv_obj_set_size(row, LV_PCT(100), 45);
  lv_obj_set_pos(row, 0, y);
}

lv_obj_t *create_row_label(lv_obj_t *row, const char *text) {
  lv_obj_t *label = lv_label_create(row);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_align(label, LV_ALIGN_LEFT_MID, 22, 0);
  return label;
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
  , brand_label(lv_label_create(cont))
  , version_label(lv_label_create(cont))
  , update_button(lv_btn_create(cont))
  , update_button_label(lv_label_create(update_button))
  , update_status(lv_label_create(cont))
  , back_btn(cont, &back, "Back", &SysInfoPanel::_handle_callback, this)
{
  Config *conf = Config::get_instance();

  style_screen_object(cont);
  lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(cont, screen_background_color(), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, LV_PART_MAIN);

  lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(title_bar, LV_PCT(100), 32);
  lv_obj_set_pos(title_bar, 0, 0);
  lv_obj_set_style_pad_all(title_bar, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_color(title_bar, lv_color_hex(BUTTON_GREY), LV_PART_MAIN);
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
  lv_obj_set_pos(network_card, 35, 47);

  lv_label_set_text(network_title_label, "Network:");
  lv_obj_set_style_text_color(network_title_label, lv_color_white(), LV_PART_MAIN);
  lv_obj_set_style_text_font(network_title_label, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_set_pos(network_title_label, 32, 8);

  lv_obj_set_width(network_name_label, 225);
  lv_obj_set_height(network_name_label, 24);
  lv_obj_set_style_text_color(network_name_label, lv_color_hex(CREALITY_GREEN), LV_PART_MAIN);
  lv_obj_set_style_text_font(network_name_label, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_label_set_long_mode(network_name_label, LV_LABEL_LONG_DOT);
  lv_obj_set_pos(network_name_label, 32, 32);

  lv_obj_set_width(network_ip_label, 225);
  lv_obj_set_height(network_ip_label, 24);
  lv_obj_set_style_text_color(network_ip_label, lv_color_hex(CREALITY_GREEN), LV_PART_MAIN);
  lv_obj_set_style_text_font(network_ip_label, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_set_pos(network_ip_label, 32, 56);

  lv_img_set_src(printer_img, &device);
  lv_obj_set_pos(printer_img, 0, 130);

  style_card(controls_card);
  lv_obj_set_size(controls_card, 410, 220);
  lv_obj_set_pos(controls_card, 360, 47);

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

  lv_label_set_text(brand_label, "PowerScreen by Boris SdK");
  lv_obj_set_style_text_color(brand_label, lv_color_white(), LV_PART_MAIN);
  lv_obj_set_style_text_font(brand_label, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_set_pos(brand_label, 376, 292);

  lv_label_set_text(version_label, fmt::format("Version: {}", GS_VERSION).c_str());
  lv_obj_set_width(version_label, 410);
  lv_obj_set_style_text_color(version_label, lv_color_white(), LV_PART_MAIN);
  lv_obj_set_style_text_font(version_label, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_label_set_long_mode(version_label, LV_LABEL_LONG_CLIP);
  lv_obj_set_pos(version_label, 376, 318);

  lv_obj_clear_flag(update_button, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(update_button, 243, 38);
  lv_obj_set_pos(update_button, 376, 367);
  lv_obj_set_style_bg_color(update_button, lv_color_hex(BUTTON_GREY),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(update_button, LV_OPA_COVER,
                          LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(update_button, lv_color_darken(lv_color_hex(BUTTON_GREY), LV_OPA_20),
                            LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(update_button, LV_OPA_COVER,
                          LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_border_width(update_button, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(update_button, 12, LV_PART_MAIN);

  lv_label_set_text(update_button_label, "Check for Updates");
  lv_obj_set_width(update_button_label, LV_PCT(100));
  lv_obj_set_style_text_align(update_button_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_style_text_color(update_button_label, lv_color_white(), LV_PART_MAIN);
  lv_obj_set_style_text_font(update_button_label, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_align(update_button_label, LV_ALIGN_CENTER, 0, 0);
  lv_obj_add_event_cb(update_button, &SysInfoPanel::_handle_callback,
                      LV_EVENT_CLICKED, this);

  lv_label_set_text(update_status, "New Update!");
  lv_obj_set_style_text_color(update_status, lv_color_hex(CARD_BORDER), LV_PART_MAIN);
  lv_obj_set_style_text_font(update_status, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_set_pos(update_status, 376, 410);
  lv_obj_add_flag(update_status, LV_OBJ_FLAG_HIDDEN);

  lv_obj_add_flag(back_btn.get_container(), LV_OBJ_FLAG_FLOATING);
  lv_obj_align(back_btn.get_container(), LV_ALIGN_BOTTOM_RIGHT, 0, -14);
  lv_obj_move_background(cont);
}

SysInfoPanel::~SysInfoPanel() {
  if (clock_timer != NULL) {
    lv_timer_del(clock_timer);
    clock_timer = NULL;
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
  check_for_update();
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

void SysInfoPanel::check_for_update() {
  lv_obj_add_flag(update_status, LV_OBJ_FLAG_HIDDEN);

  try {
    const fs::path script = fs::canonical("/proc/self/exe").parent_path() / "update.sh";
    if (!fs::exists(script)) {
      spdlog::warn("Failed to check for updates. Did not find update script.");
      return;
    }

    const std::vector<std::string> command = {script.string(), "--check"};
    const auto output = sp::check_output(command);
    const std::string result(output.buf.data(), output.length);
    if (result.rfind("UPDATE_AVAILABLE:", 0) == 0) {
      lv_obj_clear_flag(update_status, LV_OBJ_FLAG_HIDDEN);
    }
  } catch (const std::exception &error) {
    spdlog::warn("Failed to check for PowerScreen updates: {}", error.what());
  }
}

void SysInfoPanel::run_update() {
  try {
    auto update_script = fs::canonical("/proc/self/exe").parent_path() / "update.sh";
    const fs::path script(update_script);
    if (fs::exists(script)) {
      sp::call(script);
    } else {
      spdlog::warn("Failed to update PowerScreen. Did not find update script.");
    }
  } catch (const std::exception &error) {
    spdlog::warn("Failed to update PowerScreen: {}", error.what());
  }
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

    if (btn == back_btn.get_container()) {
      lv_obj_move_background(cont);
    } else if (btn == update_button) {
      spdlog::trace("update powerscreen pressed from system info");
      run_update();
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
