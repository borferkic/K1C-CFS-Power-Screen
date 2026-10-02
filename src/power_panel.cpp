#include "power_panel.h"
#include "powerui.h"
#include "utils.h"
#include "spdlog/spdlog.h"

#include <map>

LV_IMG_DECLARE(back);
LV_IMG_DECLARE(ui_icon_power);

namespace {
constexpr int ROW_HEIGHT = 76;
constexpr int LIST_HEADER = 74;
}

PowerPanel::PowerPanel(KWebSocketClient &websocket_client, std::mutex &l)
  : ws(websocket_client)
  , lv_lock(l)
  , cont(lv_obj_create(lv_scr_act()))
  , back_btn(cont, &back, "Back", &PowerPanel::_handle_callback, this)
{
  lv_obj_move_background(cont);
  powerui::style_overlay_root(cont);

  list_card = powerui::card(cont, 12, 12, 712, LIST_HEADER);
  lv_obj_t *title = powerui::label(list_card, "Power devices", &lv_font_montserrat_16, lv_color_hex(powerui::COLOR_FG));
  lv_obj_set_pos(title, powerui::px(20), powerui::px(16));
  lv_obj_t *subtitle = powerui::label(list_card, "Printer power outlets managed by Moonraker", &lv_font_montserrat_12,
                                      lv_color_hex(powerui::COLOR_MUTED));
  lv_obj_set_pos(subtitle, powerui::px(20), powerui::px(42));

  empty = powerui::empty_state(cont, &ui_icon_power, "No power devices",
                               "Add a [power] device to moonraker.conf to control a printer outlet from here.");
  lv_obj_set_pos(empty, 0, powerui::px(80));
  lv_obj_add_flag(empty, LV_OBJ_FLAG_HIDDEN);

  lv_obj_add_flag(back_btn.get_container(), LV_OBJ_FLAG_HIDDEN);  // Back lives in the title bar
}

void PowerPanel::set_state(Device &d, bool on) {
  if (on) {
    lv_obj_add_state(d.toggle, LV_STATE_CHECKED);
  } else {
    lv_obj_clear_state(d.toggle, LV_STATE_CHECKED);
  }
  powerui::badge_set(d.state, on ? "On" : "Off", lv_color_hex(on ? powerui::COLOR_ACCENT : powerui::COLOR_MUTED));
}

PowerPanel::~PowerPanel() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }

  devices.clear();
}

void PowerPanel::create_device(json &j) {
  using namespace powerui;
  std::string name = j["device"].template get<std::string>();

  auto entry = devices.find(name);
  if (entry == devices.end()) {
    const int row = static_cast<int>(devices.size());
    const int y = LIST_HEADER + row * ROW_HEIGHT;

    lv_obj_t *sep = plain(list_card);
    lv_obj_set_size(sep, lv_pct(100), 1);
    lv_obj_set_pos(sep, 0, px(y));
    lv_obj_set_style_bg_color(sep, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_bg_opa(sep, LV_OPA_10, 0);

    lv_obj_t *tile = plain(list_card);
    lv_obj_set_size(tile, px(44), px(44));
    lv_obj_set_pos(tile, px(20), px(y + 16));
    lv_obj_set_style_radius(tile, px(10), 0);
    lv_obj_set_style_bg_color(tile, lv_color_hex(COLOR_SECONDARY), 0);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
    lv_obj_center(icon(tile, &ui_icon_power, 24, lv_color_hex(COLOR_FG)));

    lv_obj_t *l = label(list_card, name.c_str(), &lv_font_montserrat_16, lv_color_hex(COLOR_FG));
    lv_obj_set_pos(l, px(78), px(y + 28));

    Device d;
    d.toggle = lv_switch_create(list_card);
    style_switch(d.toggle);
    lv_obj_set_pos(d.toggle, px(712 - 20 - 46), px(y + 25));
    lv_obj_add_event_cb(d.toggle, &PowerPanel::_handle_callback, LV_EVENT_VALUE_CHANGED, this);

    d.state = badge(list_card, "Off", lv_color_hex(COLOR_MUTED));
    lv_obj_align_to(d.state, d.toggle, LV_ALIGN_OUT_LEFT_MID, -px(12), 0);

    lv_obj_set_height(list_card, px(y + ROW_HEIGHT + 4));
    lv_obj_add_flag(empty, LV_OBJ_FLAG_HIDDEN);
    entry = devices.insert({name, d}).first;
  }

  std::string status = j["status"].template get<std::string>();
  spdlog::debug("Fetched initial status for power device {}: {}", name, status);
  set_state(entry->second, status == "on");
}

void PowerPanel::create_devices(json &j) {
  std::lock_guard<std::mutex> lock(lv_lock);

  if (j.contains("result")) {
    json result = j["result"];
    if (result.contains("devices")) {
      json devices = result["devices"];
      for (auto &device : devices.items()) {
        create_device(device.value());
      }
    }
  }
}

void PowerPanel::handle_device_callback(json &j) {
  std::lock_guard<std::mutex> lock(lv_lock);

  if (j.contains("result")) {
    json result = j["result"];
    for (auto &device : result.items()) {
      auto entry = devices.find(device.key());
      if (entry != devices.end()) {
        std::string new_status = device.value();

        spdlog::debug("Fetched new status for power device {}: {}", entry->first, new_status);

        set_state(entry->second, new_status == "on");
      }
    }
  }
}

void PowerPanel::foreground() {
  lv_obj_move_foreground(cont);
  if (devices.empty()) {
    lv_obj_clear_flag(empty, LV_OBJ_FLAG_HIDDEN);
  }

  json params;
  for (auto &device : devices) {
    params[device.first] = 0; // value is ignored by moonraker
  }
  ws.send_jsonrpc("machine.device_power.status", params, [this](json& j) {
    this->handle_device_callback(j);
  });
  lv_obj_add_flag(back_btn.get_container(), LV_OBJ_FLAG_HIDDEN);  // Back lives in the title bar
  powerui::overlay_open("Power Devices", [this]() { lv_obj_move_background(cont); });
}

void PowerPanel::handle_callback(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    lv_obj_t *btn = lv_event_get_current_target(e);
    if (btn == back_btn.get_container()) {
      lv_obj_move_background(cont);
    }
  } else if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
    lv_obj_t *obj = lv_event_get_target(e);
    for (auto &device : devices) {
      if (obj == device.second.toggle) {
        bool turnOn = lv_obj_has_state(device.second.toggle, LV_STATE_CHECKED);
        powerui::badge_set(device.second.state, turnOn ? "On" : "Off",
                           lv_color_hex(turnOn ? powerui::COLOR_ACCENT : powerui::COLOR_MUTED));
        std::string status = turnOn ? "on" : "off";

        spdlog::debug("Turning power device {} {}", device.first, status);

        json params;
        params["device"] = device.first;
        params["action"] = status;
        ws.send_jsonrpc("machine.device_power.post_device", params, [this](json& j) {
          this->handle_device_callback(j);
        });
	      break;
      }
    }
  }
}
