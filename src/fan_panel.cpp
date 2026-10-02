#include "fan_panel.h"
#include "powerui.h"
#include "state.h"
#include "utils.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <vector>

LV_IMG_DECLARE(back);

namespace {
constexpr uint32_t FAN_PANEL_BACKGROUND = powerui::COLOR_BG;
}

FanPanel::FanPanel(KWebSocketClient &websocket_client, std::mutex &lock)
  : NotifyConsumer(lock)
  , ws(websocket_client)
  , fanpanel_cont(lv_obj_create(lv_scr_act()))
  , title_bar(lv_obj_create(fanpanel_cont))
  , title_label(lv_label_create(title_bar))
  , time_label(lv_label_create(title_bar))
  , clock_timer(NULL)
  , fans_cont(lv_obj_create(fanpanel_cont))
  , back_btn(fanpanel_cont, &back, "Back", &FanPanel::_handle_callback, this)
{
  lv_obj_set_style_pad_all(fanpanel_cont, 0, 0);
  lv_obj_clear_flag(fanpanel_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(fanpanel_cont, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(fanpanel_cont, lv_color_hex(FAN_PANEL_BACKGROUND), 0);
  lv_obj_set_style_bg_opa(fanpanel_cont, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(fanpanel_cont, 0, 0);

  lv_obj_add_flag(title_bar, LV_OBJ_FLAG_HIDDEN);  // the main title bar shows "Fans"
  lv_obj_set_style_bg_opa(title_bar, LV_OPA_TRANSP, 0);
  lv_obj_set_size(title_bar, 1, 1);
  clock_timer = lv_timer_create(&FanPanel::_update_clock_cb, 1000, this);

  lv_obj_clear_flag(fans_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(fans_cont, LV_PCT(100), LV_PCT(100));
  lv_obj_set_pos(fans_cont, 0, 0);
  lv_obj_set_style_pad_all(fans_cont, 0, 0);
  lv_obj_set_style_border_width(fans_cont, 0, 0);
  lv_obj_set_style_bg_opa(fans_cont, LV_OPA_TRANSP, 0);
  lv_obj_add_flag(back_btn.get_container(), LV_OBJ_FLAG_HIDDEN);
  ws.register_notify_update(this);
}

FanPanel::~FanPanel() {
  if (clock_timer != NULL) {
    lv_timer_del(clock_timer);
    clock_timer = NULL;
  }

  if (fanpanel_cont != NULL) {
    lv_obj_del(fanpanel_cont);
    fanpanel_cont = NULL;
  }

  fans.clear();

  ws.unregister_notify_update(this);
}

void FanPanel::consume(json &j) {
  std::lock_guard<std::mutex> lock(lv_lock);
  for (auto &f : fans) {
    // hack for output_pin fans
    auto fan_value = j[json::json_pointer(fmt::format("/params/0/{}/value", f.first))];
    if (!fan_value.is_null()) {
      int v = static_cast<int>(fan_value.template get<double>() * 100);
      f.second->update_value(v);
    }

    fan_value = j[json::json_pointer(fmt::format("/params/0/{}/speed", f.first))];
    if (!fan_value.is_null()) {
      int v = static_cast<int>(fan_value.template get<double>() * 100);
      f.second->update_value(v);
    }
  }
}

void FanPanel::create_fans(json &f) {
  std::lock_guard<std::mutex> lock(lv_lock);
  fans.clear();

  // Hotend, Chamber and Rear first (the K1C's fan0, fan2 and fan1); any other fan follows.
  const std::vector<std::pair<std::string, std::string>> known = {
    {"output_pin fan0", "Hotend Fan"}, {"output_pin fan2", "Chamber Fan"}, {"output_pin fan1", "Rear Fan"}};
  std::vector<std::pair<std::string, std::string>> order;
  for (const auto &k : known) {
    if (f.contains(k.first)) {
      order.push_back(k);
    }
  }
  for (auto &fan : f.items()) {
    const std::string key = fan.key();
    bool listed = false;
    for (const auto &k : known) {
      listed = listed || k.first == key;
    }
    if (!listed) {
      order.push_back({key, fan.value()["display_name"].template get<std::string>()});
    }
  }

  const int count = static_cast<int>(order.size());
  const int card_height = count > 3 ? 132 : (416 - 12 * (count - 1)) / std::max(count, 1);
  int index = 0;
  for (const auto &entry : order) {
    const std::string &key = entry.first;
    spdlog::trace("create fan {}", key);

    lv_event_cb_t fan_cb = &FanPanel::_handle_fan_update;
    if (key == "fan") {
      fan_cb = &FanPanel::_handle_fan_update_part_fan;
    } else if (key.rfind("output_pin ", 0) != 0) {
      // generic_fan, controller_fan, etc.
      fan_cb = &FanPanel::_handle_fan_update_generic;
    }
    auto fptr = std::make_shared<FanControl>(fans_cont, entry.second.c_str(), 12 + index * (card_height + 12), card_height, fan_cb, this);
    fans.insert({key, fptr});
    index++;
  }

  if (fans.size() > 3) {
    lv_obj_add_flag(fans_cont, LV_OBJ_FLAG_SCROLLABLE);
  } else {
    lv_obj_clear_flag(fans_cont, LV_OBJ_FLAG_SCROLLABLE);    
  }

  lv_obj_move_foreground(back_btn.get_container());
}

void FanPanel::foreground() {
  for (auto &f : fans) {
    // hack for output_pin fans
    auto fan_value = State::get_instance()
      ->get_data(json::json_pointer(fmt::format("/printer_state/{}/value", f.first)));
    if (!fan_value.is_null()) {
      int v = static_cast<int>(fan_value.template get<double>() * 100);
      f.second->update_value(v);
    }

    fan_value = State::get_instance()
      ->get_data(json::json_pointer(fmt::format("/printer_state/{}/speed", f.first)));
    if (!fan_value.is_null()) {
      int v = static_cast<int>(fan_value.template get<double>() * 100);
      f.second->update_value(v);
    }
  }
  
  lv_obj_move_foreground(back_btn.get_container());
  lv_obj_move_foreground(fanpanel_cont);
  lv_obj_add_flag(back_btn.get_container(), LV_OBJ_FLAG_HIDDEN);  // Back lives in the title bar
  powerui::overlay_open("Fans", [this]() { lv_obj_move_background(fanpanel_cont); });
}

void FanPanel::update_clock() {
  const std::time_t now = std::time(nullptr);
  const std::tm local_time = *std::localtime(&now);
  char time_text[6] = {};
  std::strftime(time_text, sizeof(time_text), "%H:%M", &local_time);
  lv_label_set_text(time_label, time_text);
}

void FanPanel::handle_callback(lv_event_t *event) {
  lv_obj_t *btn = lv_event_get_current_target(event);
  if (btn == back_btn.get_container()) {
    lv_obj_move_background(fanpanel_cont);
  }
  else {
    spdlog::debug("Unknown action button pressed");
  }
}

void FanPanel::handle_fan_update(lv_event_t *event) {
  lv_obj_t *obj = lv_event_get_target(event);

  if (lv_event_get_code(event) == LV_EVENT_RELEASED) {
    double pct = 255 * (double)lv_slider_get_value(obj) / 100.0;

    spdlog::trace("updating fan speed to {}", pct);
    for (auto &f : fans) {
      if (obj == f.second->get_slider()) {
	std::string fan_name = KUtils::get_obj_name(f.first);
      	spdlog::trace("update fan {}", fan_name);
	ws.gcode_script(fmt::format(fmt::format("SET_PIN PIN={} VALUE={}", fan_name, pct)));
	break;
      }
    }
  } else if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
    obj = lv_event_get_current_target(event);
    for (auto &f : fans) {
      if (obj == f.second->get_off()) {
	std::string fan_name = KUtils::get_obj_name(f.first);
      	spdlog::trace("turning off fan {}", fan_name);
	ws.gcode_script(fmt::format("SET_PIN PIN={} VALUE=0", fan_name));
	f.second->update_value(0);
	break;
      } else if (obj == f.second->get_max()) {
	std::string fan_name = KUtils::get_obj_name(f.first);
	spdlog::trace("turning fan to max {}", fan_name);
	ws.gcode_script(fmt::format("SET_PIN PIN={} VALUE=255", fan_name));
	f.second->update_value(100);
	break;
      }
    }
  }
}

void FanPanel::handle_fan_update_part_fan(lv_event_t *event) {
  lv_obj_t *obj = lv_event_get_target(event);

  if (lv_event_get_code(event) == LV_EVENT_RELEASED) {
    double pct = 255 * (double)lv_slider_get_value(obj) / 100.0;

    spdlog::trace("updating part fan speed to {}", pct);
    for (auto &f : fans) {
      if (obj == f.second->get_slider()) {
	ws.gcode_script(fmt::format(fmt::format("M106 S{}", pct)));
	break;
      }
    }

  } else if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
    obj = lv_event_get_current_target(event);
    
    for (auto &f : fans) {
      if (obj == f.second->get_off()) {
	ws.gcode_script("M106 S0");
	f.second->update_value(0);
	break;
      } else if (obj == f.second->get_max()) {
	ws.gcode_script("M106 S255");
	f.second->update_value(100);
	break;
      }
    }
  }
}

void FanPanel::handle_fan_update_generic(lv_event_t *event) {
  lv_obj_t *obj = lv_event_get_target(event);

  if (lv_event_get_code(event) == LV_EVENT_RELEASED) {
    double pct = (double)lv_slider_get_value(obj) / 100.0;

    spdlog::trace("updating fan speed to {}", pct);
    for (auto &f : fans) {
      if (obj == f.second->get_slider()) {
	std::string fan_name = KUtils::get_obj_name(f.first);
      	spdlog::trace("update fan {}", fan_name);
	ws.gcode_script(fmt::format(fmt::format("SET_FAN_SPEED FAN={} SPEED={}", fan_name, pct)));
	break;
      }
    }
  } else if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
    obj = lv_event_get_current_target(event);

    for (auto &f : fans) {
      if (obj == f.second->get_off()) {
	std::string fan_name = KUtils::get_obj_name(f.first);
      	spdlog::trace("turning off fan {}", fan_name);
	ws.gcode_script(fmt::format("SET_FAN_SPEED FAN={} SPEED=0", fan_name));
	f.second->update_value(0);
	break;
      } else if (obj == f.second->get_max()) {
	std::string fan_name = KUtils::get_obj_name(f.first);
	spdlog::trace("turning fan to max {}", fan_name);
	ws.gcode_script(fmt::format("SET_FAN_SPEED FAN={} SPEED=1", fan_name));
	f.second->update_value(100);
	break;
      }
    }
  }
}
