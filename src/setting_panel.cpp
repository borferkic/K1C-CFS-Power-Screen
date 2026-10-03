#include "setting_panel.h"
#include "powerui.h"
#include "config.h"
#include "state.h"
#include "spdlog/spdlog.h"
#include "subprocess.hpp"

#include <experimental/filesystem>

namespace fs = std::experimental::filesystem;
namespace sp = subprocess;

LV_IMG_DECLARE(network_img);
LV_IMG_DECLARE(refresh_img);
LV_IMG_DECLARE(ui_cfs_img);
LV_IMG_DECLARE(update_img);

LV_IMG_DECLARE(info_img);

LV_IMG_DECLARE(print);
LV_IMG_DECLARE(ui_console_img);

SettingPanel::SettingPanel(KWebSocketClient &c, std::mutex &l, lv_obj_t *parent, SpoolmanPanel &sm)
  : ws(c)
  , cont(lv_obj_create(parent))
#ifndef OS_ANDROID
  , wifi_panel(l)
#endif
  , sysinfo_panel()
  , spoolman_panel(sm)
  , wifi_btn(cont, &network_img, "Wi-Fi", &SettingPanel::_handle_callback, this)
  , restart_klipper_btn(cont, &refresh_img, "Restart Klipper", &SettingPanel::_handle_callback, this)
  , restart_firmware_btn(cont, &refresh_img, "Restart Firmware", &SettingPanel::_handle_callback, this)
  , sysinfo_btn(cont, &info_img, "System", &SettingPanel::_handle_callback, this)
  , spoolman_btn(cont, &ui_cfs_img, "CFS", &SettingPanel::_handle_callback, this)
  , powerscreen_restart_btn(cont, &refresh_img, "Restart Screen", &SettingPanel::_handle_callback, this)
  , powerscreen_update_btn(cont, &update_img, "Power Update", &SettingPanel::_handle_callback, this)
  , printer_select_btn(cont, &print, "Printers", &SettingPanel::_handle_callback, this)
  , update_lock_timer(NULL)
  , update_locked(false)
{
  lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));

  sysinfo_panel.set_websocket(&ws);
  sysinfo_panel.set_wifi_callback([this]() { wifi_panel.foreground(); });
  wifi_btn.set_subtitle("Network");
  restart_klipper_btn.set_subtitle("Reload printer service");
  restart_firmware_btn.set_subtitle("Firmware restart");
  sysinfo_btn.set_subtitle("Preferences and info");
  spoolman_btn.set_subtitle("Filament system");
  powerscreen_restart_btn.set_subtitle("Reload this interface");
  powerscreen_update_btn.set_subtitle("PowerScreen and Script");

  spoolman_btn.disable();
  spoolman_btn.set_pill("Offline", lv_color_hex(powerui::COLOR_DESTRUCTIVE));
  update_lock_timer = lv_timer_create(&SettingPanel::_refresh_update_lock_cb, 1000, this);
  refresh_update_lock();
#ifdef OS_ANDROID
  wifi_btn.disable();
#endif

  static lv_coord_t grid_main_row_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
  static lv_coord_t grid_main_col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
      LV_GRID_TEMPLATE_LAST};

  lv_obj_set_grid_dsc_array(cont, grid_main_col_dsc, grid_main_row_dsc);
  lv_obj_set_style_pad_all(cont, 12, LV_PART_MAIN);
  lv_obj_set_style_pad_row(cont, 12, LV_PART_MAIN);
  lv_obj_set_style_pad_column(cont, 12, LV_PART_MAIN);

  const lv_grid_align_t S = LV_GRID_ALIGN_STRETCH;
  // row 1
  lv_obj_set_grid_cell(wifi_btn.get_button(), S, 0, 1, S, 0, 1);
  lv_obj_set_grid_cell(restart_klipper_btn.get_button(), S, 1, 1, S, 0, 1);
  lv_obj_set_grid_cell(restart_firmware_btn.get_button(), S, 2, 1, S, 0, 1);
  lv_obj_set_grid_cell(sysinfo_btn.get_button(), S, 3, 1, S, 0, 1);

  // row 2 (the Console now lives in Calibrations; this build only targets the K1C, so Printers stays hidden)
  lv_obj_set_grid_cell(spoolman_btn.get_button(), S, 0, 1, S, 1, 1);
  lv_obj_set_grid_cell(powerscreen_restart_btn.get_button(), S, 1, 1, S, 1, 1);
  lv_obj_set_grid_cell(powerscreen_update_btn.get_button(), S, 2, 1, S, 1, 1);
  lv_obj_add_flag(printer_select_btn.get_button(), LV_OBJ_FLAG_HIDDEN);

}

SettingPanel::~SettingPanel() {
  if (update_lock_timer != NULL) {
    lv_timer_del(update_lock_timer);
    update_lock_timer = NULL;
  }

  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

lv_obj_t *SettingPanel::get_container() {
  return cont;
}

void SettingPanel::handle_callback(lv_event_t *event) {
  if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
    lv_obj_t *btn = lv_event_get_current_target(event);

    if (btn == wifi_btn.get_button()) {
      spdlog::trace("wifi pressed");
#ifndef OS_ANDROID
      wifi_panel.foreground();
#endif
    } else if (btn == sysinfo_btn.get_button()) {
      spdlog::trace("setting system info pressed");
      sysinfo_panel.foreground();
    } else if (btn == restart_klipper_btn.get_button()) {
      spdlog::trace("setting restart klipper pressed");
      show_confirm("Restart Klipper?", "printer.restart");
    } else if (btn == restart_firmware_btn.get_button()) {
      spdlog::trace("setting restart firmware pressed");
      show_confirm("Restart Firmware?", "printer.firmware_restart");
    } else if (btn == spoolman_btn.get_button()) {
      spdlog::trace("setting spoolman pressed");
      spoolman_panel.foreground();
    } else if (btn == powerscreen_restart_btn.get_button()) {
      spdlog::trace("restart powerscreen pressed");
      Config *conf = Config::get_instance();
      auto init_script = conf->get<std::string>("/powerscreen_init_script");
      const fs::path script(init_script);
      if (fs::exists(script) || init_script.rfind("service powerscreen", 0) == 0) {
        sp::call({init_script, "restart"});
      } else {
        spdlog::warn("Failed to restart PowerScreen. Did not find restart script.");
      }
    } else if (btn == powerscreen_update_btn.get_button()) {
      spdlog::trace("update powerscreen pressed");
      sysinfo_panel.open_power_update();
    } else if (btn == printer_select_btn.get_button()) {
      spdlog::trace("setting printers pressed");
      printer_select_panel.foreground();
    }
  }
}

// Disables the Power Update tile and shows a "Printing" pill while a print is running or paused; restores it when the
// print ends. Runs every second on the LVGL thread.
void SettingPanel::refresh_update_lock() {
  bool busy = false;
  auto &pstate = State::get_instance()->get_data("/printer_state/print_stats/state"_json_pointer);
  if (pstate.is_string()) {
    const std::string state = pstate.template get<std::string>();
    busy = state == "printing" || state == "paused";
  }
  if (busy == update_locked) {
    return;
  }
  update_locked = busy;
  if (busy) {
    powerscreen_update_btn.disable();
    powerscreen_update_btn.set_pill("Printing", lv_color_hex(powerui::COLOR_WARNING));
  } else {
    powerscreen_update_btn.enable();
    powerscreen_update_btn.hide_pill();
  }
}

void SettingPanel::enable_spoolman() {
  spoolman_btn.enable();
  spoolman_btn.hide_pill();
}

void SettingPanel::show_confirm(const std::string &title, const std::string &method) {
  std::string text = method == "printer.restart" ? "Klipper will restart." : "The printer firmware will restart.";
  auto &pstate = State::get_instance()->get_data("/printer_state/print_stats/state"_json_pointer);
  if (pstate.is_string()) {
    const std::string state = pstate.template get<std::string>();
    if (state == "printing" || state == "paused") {
      text += " This will cancel the current print.";
    }
  }
  powerui::confirm_dialog(title.c_str(), text.c_str(), "Restart", powerui::ActionKind::Destructive,
                          [this, method]() {
                            spdlog::debug("confirmed {}", method);
                            ws.send_jsonrpc(method);
                          });
}
