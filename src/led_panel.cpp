#include "led_panel.h"
#include "state.h"
#include "utils.h"
#include "spdlog/spdlog.h"

LedPanel::LedPanel(KWebSocketClient &websocket_client, std::mutex &lock)
  : NotifyConsumer(lock)
  , ws(websocket_client)
{
  ws.register_notify_update(this);
}

LedPanel::~LedPanel() {
  ws.unregister_notify_update(this);
}

void LedPanel::consume(json &j) {
  std::lock_guard<std::mutex> lock(lv_lock);
  bool state_found = false;

  for (auto &led : leds) {
    auto value = j[json::json_pointer(
        fmt::format("/params/0/{}/value", led.first))];
    if (!value.is_null()) {
      led.second = value.template get<double>() > 0.0;
      state_found = true;
    }

    value = j[json::json_pointer(
        fmt::format("/params/0/{}/color_data", led.first))];
    if (!value.is_null() && value.size() > 0) {
      // color_data = [[r,b,g,w]]
      value = value.at(0);
      if (value.size() == 4) {
        led.second = value.at(3).template get<double>() > 0.0;
        state_found = true;
      }
    }
  }

  if (state_found) {
    notify_state();
  }
}

void LedPanel::init(json &configured_leds) {
  std::lock_guard<std::mutex> lock(lv_lock);
  leds.clear();

  for (auto &led : configured_leds.items()) {
    leds.insert({led.key(), false});
    spdlog::trace("register led {}", led.key());
  }

  notify_state();
}

void LedPanel::refresh() {
  for (auto &led : leds) {
    auto value = State::get_instance()->get_data(
        json::json_pointer(fmt::format("/printer_state/{}/value", led.first)));
    if (!value.is_null()) {
      led.second = value.template get<double>() > 0.0;
      continue;
    }

    value = State::get_instance()->get_data(
        json::json_pointer(fmt::format("/printer_state/{}/color_data", led.first)));
    if (!value.is_null() && value.size() > 0) {
      // color_data = [[r,b,g,w]]
      value = value.at(0);
      if (value.size() == 4) {
        led.second = value.at(3).template get<double>() > 0.0;
      }
    }
  }

  notify_state();
}

void LedPanel::toggle() {
  bool any_led_on = false;
  for (const auto &led : leds) {
    any_led_on = any_led_on || led.second;
  }

  const bool turn_on = !any_led_on;
  const double value = turn_on ? 1.0 : 0.0;

  for (auto &led : leds) {
    spdlog::debug("turning {} led {}", turn_on ? "on" : "off", led.first);
    send_value(led.first, value);
    led.second = turn_on;
  }

  notify_state();
}

void LedPanel::set_state_callback(std::function<void(bool)> callback) {
  state_callback = std::move(callback);
}

void LedPanel::notify_state() {
  if (state_callback) {
    bool any_led_on = false;
    for (const auto &led : leds) {
      any_led_on = any_led_on || led.second;
    }
    state_callback(any_led_on);
  }
}

void LedPanel::send_value(const std::string &id, double value) {
  std::string led_name = KUtils::get_obj_name(id);
  if (id.rfind("output_pin ", 0) == 0) {
    ws.gcode_script(fmt::format("SET_PIN PIN={} VALUE={}", led_name, value));
  } else {
    ws.gcode_script(fmt::format("SET_LED LED={} WHITE={}", led_name, value));
  }
}
