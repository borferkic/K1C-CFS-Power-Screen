#include "sysinfo_panel.h"
#include "utils.h"
#include "config.h"
#include "state.h"
#include "spdlog/spdlog.h"
#include "subprocess.hpp"

#include <algorithm>
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

#ifdef POWERSCREEN_VERSION
#define GS_VERSION POWERSCREEN_VERSION
#else
#define GS_VERSION "dev-snapshot"
#endif

namespace {
// Version del paquete instalado (.version junto al ejecutable). Una release
// oficial promovida reutiliza el binario de la nightly, asi que la version
// compilada solo sirve de respaldo.
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

// Canal de actualizacion elegido en System: "nightly" (por defecto) o "stable".
std::string update_channel() {
  auto &v = Config::get_instance()->get_json("/update_channel");
  if (v.is_string() && v.get<std::string>() == "stable") {
    return "stable";
  }
  return "nightly";
}

constexpr uint32_t CARD_BORDER = 0x4CAF50;
constexpr uint32_t CREALITY_GREEN = 0x4CAF50;
constexpr uint32_t BUTTON_GREY = 0x555555;
constexpr uint32_t SCREEN_BACKGROUND = 0x282B30;
constexpr uint32_t BACK_BUTTON_BACKGROUND = SCREEN_BACKGROUND;

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
  lv_obj_set_style_translate_y(label, 5, LV_PART_MAIN);
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
  , channel_cont(lv_obj_create(controls_card))
  , channel_dd(lv_dropdown_create(channel_cont))
  , brand_label(lv_label_create(cont))
  , version_label(lv_label_create(cont))
  , update_button(lv_btn_create(cont))
  , update_button_label(lv_label_create(update_button))
  , update_status(lv_label_create(cont))
  , back_btn(cont, &back, "Back", &SysInfoPanel::_handle_callback, this)
  , update_overlay(NULL)
  , update_spinner(NULL)
  , update_title(NULL)
  , update_phase(NULL)
  , update_close_btn(NULL)
  , update_timer(NULL)
  , check_running(false)
  , check_done(false)
  , check_available(false)
  , update_running(false)
  , update_finished(false)
  , update_exit_code(0)
{
  Config *conf = Config::get_instance();

  style_screen_object(cont);
  lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(cont, lv_color_hex(SCREEN_BACKGROUND), LV_PART_MAIN);
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
  lv_obj_set_size(network_card, 300, 104);
  lv_obj_set_pos(network_card, 35, 47);

  lv_label_set_text(network_title_label, "Network:");
  lv_obj_set_style_text_color(network_title_label, lv_color_white(), LV_PART_MAIN);
  lv_obj_set_style_text_font(network_title_label, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_set_pos(network_title_label, 32, 18);

  lv_obj_set_width(network_name_label, 250);
  lv_obj_set_height(network_name_label, 24);
  lv_obj_set_style_text_color(network_name_label, lv_color_hex(CREALITY_GREEN), LV_PART_MAIN);
  lv_obj_set_style_text_font(network_name_label, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_label_set_long_mode(network_name_label, LV_LABEL_LONG_DOT);
  lv_obj_set_pos(network_name_label, 32, 42);

  lv_obj_set_width(network_ip_label, 250);
  lv_obj_set_height(network_ip_label, 24);
  lv_obj_set_style_text_color(network_ip_label, lv_color_hex(CREALITY_GREEN), LV_PART_MAIN);
  lv_obj_set_style_text_font(network_ip_label, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_set_pos(network_ip_label, 32, 66);

  lv_img_set_src(printer_img, &device);
  lv_obj_set_pos(printer_img, 0, 130);
  // La columna izquierda la ocupa ahora la tarjeta PowerScreen.
  lv_obj_add_flag(printer_img, LV_OBJ_FLAG_HIDDEN);

  style_card(controls_card);
  lv_obj_set_size(controls_card, 410, 265);
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

  style_row(channel_cont, 193);
  create_row_label(channel_cont, "Update Channel");
  lv_obj_set_size(channel_dd, 130, 46);
  lv_obj_align(channel_dd, LV_ALIGN_RIGHT_MID, -22, 0);
  lv_dropdown_set_options(channel_dd, "Nightly\nStable");
  lv_dropdown_set_selected(channel_dd, update_channel() == "stable" ? 1 : 0);
  lv_obj_add_event_cb(channel_dd, &SysInfoPanel::_handle_callback,
                      LV_EVENT_VALUE_CHANGED, this);

  // Columna izquierda: tarjeta PowerScreen (marca, version y actualizacion)
  // debajo de Network; la columna derecha queda solo para ajustes.
  lv_obj_t *about_card = lv_obj_create(cont);
  style_card(about_card);
  lv_obj_set_size(about_card, 300, 222);
  lv_obj_set_pos(about_card, 35, 165);
  lv_obj_set_parent(brand_label, about_card);
  lv_obj_set_parent(version_label, about_card);
  lv_obj_set_parent(update_status, about_card);
  lv_obj_set_parent(update_button, about_card);

  lv_label_set_text(brand_label, "PowerScreen by Boris SdK");
  lv_obj_set_style_text_color(brand_label, lv_color_white(), LV_PART_MAIN);
  lv_obj_set_style_text_font(brand_label, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_set_width(brand_label, 268);
  lv_label_set_long_mode(brand_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_pos(brand_label, 16, 12);

  lv_label_set_text(version_label, fmt::format("Version:\n{}", installed_version()).c_str());
  lv_obj_set_width(version_label, 268);
  lv_obj_set_style_text_color(version_label, lv_color_white(), LV_PART_MAIN);
  lv_obj_set_style_text_font(version_label, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_label_set_long_mode(version_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_pos(version_label, 16, 48);

  lv_obj_clear_flag(update_button, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(update_button, 268, 46);
  lv_obj_set_pos(update_button, 16, 160);
  lv_obj_set_style_bg_color(update_button, lv_color_hex(BUTTON_GREY),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(update_button, LV_OPA_COVER,
                          LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(update_button, lv_color_darken(lv_color_hex(BUTTON_GREY), LV_OPA_20),
                            LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(update_button, LV_OPA_COVER,
                          LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_color(update_button, lv_color_hex(0x3A3A3A),
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

  lv_label_set_text(update_status, "NEW UPDATE AVAILABLE!");
  lv_obj_set_style_text_color(update_status, lv_color_hex(CARD_BORDER), LV_PART_MAIN);
  lv_obj_set_style_text_font(update_status, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_set_pos(update_status, 16, 128);
  lv_obj_add_flag(update_status, LV_OBJ_FLAG_HIDDEN);

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

// Hilo desacoplado con pthread_create directo. No usar std::thread: en el
// binario estatico MIPS su pthread_create debil queda en NULL y el proceso
// muere con SIGSEGV (epc = 0) al crear el hilo.
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

// Convierte "FASE:detalle" escrito por update.sh en texto para la pantalla.
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
  lv_obj_add_flag(update_status, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_state(update_button, LV_STATE_DISABLED);

  // La consulta a GitHub tarda varios segundos: se hace en otro hilo para no
  // congelar la pantalla. poll_update() aplica el resultado.
  if (check_running.exchange(true)) {
    return;
  }

  run_detached([this]() {
    bool available = false;
    try {
      const fs::path script = update_script_path();
      if (fs::exists(script)) {
        const std::vector<std::string> command = {script.string(), "--check"};
        const auto output = sp::check_output(command);
        const std::string result(output.buf.data(), output.length);
        available = result.rfind("UPDATE_AVAILABLE:", 0) == 0;
      } else {
        spdlog::warn("Failed to check for updates. Did not find update script.");
      }
    } catch (const std::exception &error) {
      spdlog::warn("Failed to check for PowerScreen updates: {}", error.what());
    }
    check_available = available;
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
  show_update_overlay("Updating PowerScreen", phase_text(""), true);

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
  if (check_done.exchange(false) && check_available) {
    lv_obj_clear_flag(update_status, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_state(update_button, LV_STATE_DISABLED);
  }

  if (!update_running) {
    return;
  }

  const std::string status = read_first_line(UPDATE_STATUS_FILE);
  if (!status.empty()) {
    lv_label_set_text(update_phase, phase_text(status).c_str());
  }

  if (!update_finished) {
    return;
  }

  // El script termino sin reiniciar PowerScreen: error o sin cambios.
  update_running = false;
  if (status.rfind("UP_TO_DATE", 0) == 0) {
    show_update_overlay("No update needed", phase_text(status), false);
  } else if (status.rfind("ERROR", 0) == 0) {
    show_update_overlay("Update failed", phase_text(status), false);
  } else if (update_exit_code != 0) {
    show_update_overlay("Update failed", "The update script did not finish.", false);
  } else {
    show_update_overlay("Update installed", "Restart PowerScreen to apply the new version.", false);
  }
}

void SysInfoPanel::create_update_overlay() {
  update_overlay = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(update_overlay);
  lv_obj_set_size(update_overlay, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(update_overlay, lv_color_black(), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(update_overlay, LV_OPA_80, LV_PART_MAIN);
  // Captura todos los toques para bloquear la interfaz durante la actualizacion.
  lv_obj_add_flag(update_overlay, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(update_overlay, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *card = lv_obj_create(update_overlay);
  lv_obj_set_size(card, 460, 260);
  lv_obj_center(card);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(card, lv_color_hex(SCREEN_BACKGROUND), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_color(card, lv_color_hex(CARD_BORDER), LV_PART_MAIN);
  lv_obj_set_style_border_width(card, 2, LV_PART_MAIN);
  lv_obj_set_style_radius(card, 12, LV_PART_MAIN);

  update_title = lv_label_create(card);
  lv_obj_set_style_text_color(update_title, lv_color_hex(CREALITY_GREEN), LV_PART_MAIN);
  lv_obj_set_style_text_font(update_title, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_align(update_title, LV_ALIGN_TOP_MID, 0, 4);

  update_spinner = lv_spinner_create(card, 1000, 60);
  lv_obj_set_size(update_spinner, 80, 80);
  lv_obj_set_style_arc_color(update_spinner, lv_color_hex(CREALITY_GREEN), LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(update_spinner, lv_color_hex(BUTTON_GREY), LV_PART_MAIN);
  lv_obj_align(update_spinner, LV_ALIGN_CENTER, 0, -10);

  update_phase = lv_label_create(card);
  lv_label_set_long_mode(update_phase, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(update_phase, LV_PCT(100));
  lv_obj_set_style_text_align(update_phase, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_style_text_color(update_phase, lv_color_white(), LV_PART_MAIN);
  lv_obj_set_style_text_font(update_phase, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_align(update_phase, LV_ALIGN_BOTTOM_MID, 0, -4);

  update_close_btn = lv_btn_create(card);
  lv_obj_set_size(update_close_btn, 160, 44);
  lv_obj_align(update_close_btn, LV_ALIGN_CENTER, 0, -10);
  lv_obj_set_style_bg_color(update_close_btn, lv_color_hex(CREALITY_GREEN), LV_PART_MAIN);
  lv_obj_set_style_radius(update_close_btn, 12, LV_PART_MAIN);
  lv_obj_t *close_label = lv_label_create(update_close_btn);
  lv_label_set_text(close_label, "Close");
  lv_obj_set_style_text_font(close_label, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_center(close_label);
  lv_obj_add_event_cb(update_close_btn, &SysInfoPanel::_handle_callback, LV_EVENT_CLICKED, this);

  lv_obj_add_flag(update_overlay, LV_OBJ_FLAG_HIDDEN);
}

void SysInfoPanel::show_update_overlay(const std::string &title, const std::string &phase, bool busy) {
  lv_label_set_text(update_title, title.c_str());
  lv_label_set_text(update_phase, phase.c_str());
  if (busy) {
    lv_obj_clear_flag(update_spinner, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(update_close_btn, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(update_spinner, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(update_close_btn, LV_OBJ_FLAG_HIDDEN);
  }
  lv_obj_clear_flag(update_overlay, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(update_overlay);
}

void SysInfoPanel::show_updated_notice() {
  // update.sh deja este archivo antes de reiniciar; la version nueva lo
  // muestra una sola vez al arrancar.
  const std::string version = read_first_line(UPDATE_DONE_FILE);
  if (version.empty()) {
    return;
  }
  std::remove(UPDATE_DONE_FILE);
  std::remove(UPDATE_STATUS_FILE);

  lv_obj_t *notice = lv_obj_create(lv_layer_top());
  lv_obj_set_size(notice, 520, LV_SIZE_CONTENT);
  lv_obj_align(notice, LV_ALIGN_TOP_MID, 0, 50);
  lv_obj_clear_flag(notice, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(notice, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_color(notice, lv_color_hex(CREALITY_GREEN), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(notice, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(notice, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(notice, 12, LV_PART_MAIN);

  lv_obj_t *label = lv_label_create(notice);
  lv_label_set_text(label, fmt::format(LV_SYMBOL_OK " PowerScreen updated to {}", version).c_str());
  lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_center(label);

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

    if (btn == back_btn.get_container()) {
      lv_obj_move_background(cont);
    } else if (btn == update_button) {
      if (lv_obj_has_state(update_button, LV_STATE_DISABLED)) {
        return;
      }
      spdlog::trace("update powerscreen pressed from system info");
      start_update();
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
        // Alinea el canal del Update Manager de Moonraker/Fluidd.
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
