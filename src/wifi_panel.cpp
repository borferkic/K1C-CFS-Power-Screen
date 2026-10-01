#include "wifi_panel.h"
#include "powerui.h"
#include "utils.h"
#include "config.h"
#include "spdlog/spdlog.h"

#include <sstream>
#include <iostream>
#include <vector>
#include <utility>
#include <algorithm>

LV_IMG_DECLARE(network_img);
LV_IMG_DECLARE(ui_icon_lock);

namespace {
using namespace powerui;

// 0-4 bars from the signal level in dBm.
int signal_level(int dbm) {
  if (dbm >= -55) return 4;
  if (dbm >= -65) return 3;
  if (dbm >= -75) return 2;
  return 1;
}

const char *signal_text(int level) {
  switch (level) {
    case 4: return "Excellent signal";
    case 3: return "Good signal";
    case 2: return "Fair signal";
    default: return "Weak signal";
  }
}

// Four rising bars; the first `level` are filled.
void fill_bars(lv_obj_t *bars, int level, lv_color_t color) {
  lv_obj_clean(bars);
  const int heights[4] = {6, 10, 14, 18};
  for (int i = 0; i < 4; ++i) {
    lv_obj_t *bar = plain(bars);
    lv_obj_set_size(bar, px(4), px(heights[i]));
    lv_obj_set_style_radius(bar, px(2), 0);
    lv_obj_set_style_bg_color(bar, i < level ? color : lv_color_hex(COLOR_SECONDARY), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
  }
}

lv_obj_t *make_bars(lv_obj_t *parent) {
  lv_obj_t *bars = plain(parent);
  lv_obj_set_size(bars, px(21), px(18));
  lv_obj_set_flex_flow(bars, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(bars, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
  lv_obj_set_style_pad_column(bars, px(3), 0);
  return bars;
}

void style_action_button(lv_obj_t *button, lv_color_t bg, lv_opa_t bg_opa, lv_color_t pressed, lv_color_t border, lv_opa_t border_opa, int radius) {
  lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(button, bg, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(button, bg_opa, LV_PART_MAIN);
  lv_obj_set_style_bg_color(button, pressed, LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_border_width(button, border_opa == LV_OPA_TRANSP ? 0 : 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(button, border, LV_PART_MAIN);
  lv_obj_set_style_border_opa(button, border_opa, LV_PART_MAIN);
  lv_obj_set_style_radius(button, px(radius), LV_PART_MAIN);
  lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);
}
}

WifiPanel::WifiPanel(std::mutex &l)
  : lv_lock(l)
  , cont(lv_obj_create(lv_scr_act()))
  , spinner(NULL)
  , list_card(NULL)
  , list(NULL)
  , refresh_btn(NULL)
  , status_card(NULL)
  , status_badge(NULL)
  , ssid_label(NULL)
  , ip_label(NULL)
  , signal_bars(NULL)
  , signal_label(NULL)
  , security_label(NULL)
  , connect_btn(NULL)
  , connect_label(NULL)
  , scan_btn(NULL)
  , forget_btn(NULL)
  , password_cont(NULL)
  , password_label(NULL)
  , password_input(NULL)
  , kb(NULL)
{
  lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
  lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(cont, 0, 0);
  lv_obj_set_style_radius(cont, 0, 0);
  lv_obj_set_style_border_width(cont, 0, 0);
  lv_obj_set_style_bg_color(cont, lv_color_hex(COLOR_BG), 0);
  lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, 0);
  lv_obj_add_flag(cont, LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_CLICKABLE);

  // ---- Networks card (left).
  list_card = card(cont, 12, 12, 420, 416);
  lv_obj_t *list_title = label(list_card, "Networks", &lv_font_montserrat_16, lv_color_hex(COLOR_FG));
  lv_obj_set_pos(list_title, px(20), px(24));
  refresh_btn = lv_btn_create(list_card);
  lv_obj_set_size(refresh_btn, px(36), px(36));
  lv_obj_align(refresh_btn, LV_ALIGN_TOP_RIGHT, -px(14), px(14));
  style_action_button(refresh_btn, lv_color_hex(COLOR_SECONDARY), LV_OPA_COVER, lv_color_hex(0x333333), lv_color_white(), LV_OPA_TRANSP, 8);
  lv_obj_t *refresh_label = label(refresh_btn, LV_SYMBOL_REFRESH, &lv_font_montserrat_16, lv_color_hex(COLOR_FG));
  lv_obj_center(refresh_label);
  lv_obj_add_event_cb(refresh_btn, &WifiPanel::_handle_action, LV_EVENT_CLICKED, this);

  list = lv_obj_create(list_card);
  lv_obj_remove_style_all(list);
  lv_obj_set_size(list, px(396), px(344));
  lv_obj_set_pos(list, px(12), px(60));
  lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(list, LV_DIR_VER);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(list, px(4), 0);

  // ---- Status card (right).
  status_card = card(cont, 444, 12, 280, 224);
  lv_obj_t *tile = plain(status_card);
  lv_obj_set_size(tile, px(56), px(56));
  lv_obj_set_pos(tile, px(20), px(18));
  lv_obj_set_style_radius(tile, px(12), 0);
  lv_obj_set_style_bg_color(tile, lv_color_hex(COLOR_SECONDARY), 0);
  lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
  lv_obj_t *tile_icon = icon(tile, &network_img, 34, lv_color_hex(COLOR_ACCENT));
  lv_obj_center(tile_icon);
  status_badge = badge(status_card, "Not connected", lv_color_hex(COLOR_MUTED));
  lv_obj_align(status_badge, LV_ALIGN_TOP_RIGHT, -px(16), px(18));

  ssid_label = label(status_card, "Select a network", &lv_font_montserrat_24, lv_color_hex(COLOR_FG));
  lv_obj_set_width(ssid_label, px(240));
  lv_label_set_long_mode(ssid_label, LV_LABEL_LONG_CLIP);
  lv_obj_set_pos(ssid_label, px(20), px(90));
  ip_label = label(status_card, "", &lv_font_montserrat_14, lv_color_hex(COLOR_MUTED));
  lv_obj_set_pos(ip_label, px(20), px(124));
  signal_bars = make_bars(status_card);
  lv_obj_set_pos(signal_bars, px(20), px(156));
  signal_label = label(status_card, "", &lv_font_montserrat_14, lv_color_hex(COLOR_MUTED));
  lv_obj_set_pos(signal_label, px(50), px(156));
  security_label = label(status_card, "", &lv_font_montserrat_12, lv_color_hex(COLOR_MUTED));
  lv_obj_set_pos(security_label, px(20), px(188));

  // Password prompt (shown instead of the details when a new secured network is chosen).
  password_cont = plain(status_card);
  lv_obj_set_size(password_cont, px(248), px(120));
  lv_obj_set_pos(password_cont, px(16), px(86));
  lv_obj_add_flag(password_cont, LV_OBJ_FLAG_HIDDEN);
  password_label = label(password_cont, "Password", &lv_font_montserrat_14, lv_color_hex(COLOR_MUTED));
  lv_obj_set_pos(password_label, 0, 0);
  password_input = lv_textarea_create(password_cont);
  lv_obj_set_size(password_input, px(248), px(48));
  lv_obj_set_pos(password_input, 0, px(26));
  lv_textarea_set_password_mode(password_input, true);
  lv_textarea_set_one_line(password_input, true);
  lv_obj_set_style_bg_color(password_input, lv_color_hex(COLOR_SECONDARY), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(password_input, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(password_input, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(password_input, lv_color_hex(COLOR_ACCENT), LV_PART_MAIN | LV_STATE_FOCUSED);
  lv_obj_set_style_radius(password_input, px(10), LV_PART_MAIN);
  lv_obj_set_style_text_color(password_input, lv_color_hex(COLOR_FG), LV_PART_MAIN);
  lv_obj_set_style_text_font(password_input, &lv_font_montserrat_16, LV_PART_MAIN);

  // ---- Actions under the status card.
  connect_btn = lv_btn_create(cont);
  lv_obj_set_size(connect_btn, px(280), px(56));
  lv_obj_set_pos(connect_btn, px(444), px(248));
  style_action_button(connect_btn, lv_color_hex(0x16A34A), LV_OPA_COVER, lv_color_hex(0x15803D), lv_color_white(), LV_OPA_TRANSP, 10);
  connect_label = label(connect_btn, "Connect to network", &lv_font_montserrat_16, lv_color_white());
  lv_obj_center(connect_label);
  lv_obj_add_event_cb(connect_btn, &WifiPanel::_handle_action, LV_EVENT_CLICKED, this);

  scan_btn = lv_btn_create(cont);
  lv_obj_set_size(scan_btn, px(280), px(56));
  lv_obj_set_pos(scan_btn, px(444), px(316));
  style_action_button(scan_btn, lv_color_hex(COLOR_CARD), LV_OPA_COVER, lv_color_hex(COLOR_SECONDARY), lv_color_white(), LV_OPA_10, 10);
  lv_obj_t *scan_label = label(scan_btn, LV_SYMBOL_REFRESH "  Scan again", &lv_font_montserrat_16, lv_color_hex(COLOR_FG));
  lv_obj_center(scan_label);
  lv_obj_add_event_cb(scan_btn, &WifiPanel::_handle_action, LV_EVENT_CLICKED, this);

  forget_btn = lv_btn_create(cont);
  lv_obj_set_size(forget_btn, px(280), px(44));
  lv_obj_set_pos(forget_btn, px(444), px(384));
  style_action_button(forget_btn, lv_color_hex(COLOR_CARD), LV_OPA_COVER, lv_color_hex(COLOR_SECONDARY), lv_color_hex(COLOR_DESTRUCTIVE), LV_OPA_40, 10);
  lv_obj_t *forget_label = label(forget_btn, "Forget network", &lv_font_montserrat_14, lv_color_hex(COLOR_DESTRUCTIVE));
  lv_obj_center(forget_label);
  lv_obj_add_event_cb(forget_btn, &WifiPanel::_handle_action, LV_EVENT_CLICKED, this);

  // ---- Keyboard and spinner.
  kb = lv_keyboard_create(cont);
  lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
  lv_obj_set_size(kb, LV_PCT(100), px(220));
  lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_bg_color(kb, lv_color_hex(COLOR_CARD), LV_PART_MAIN);
  lv_obj_set_style_bg_color(kb, lv_color_hex(COLOR_SECONDARY), LV_PART_ITEMS);
  lv_obj_set_style_text_color(kb, lv_color_hex(COLOR_FG), LV_PART_ITEMS);
  lv_obj_set_style_radius(kb, px(8), LV_PART_ITEMS);
  lv_obj_set_style_border_width(kb, 0, LV_PART_ITEMS);
  lv_obj_add_event_cb(password_input, &WifiPanel::_handle_kb_input, LV_EVENT_FOCUSED, this);
  lv_obj_add_event_cb(password_input, &WifiPanel::_handle_kb_input, LV_EVENT_DEFOCUSED, this);
  lv_obj_add_event_cb(password_input, &WifiPanel::_handle_kb_input, LV_EVENT_READY, this);
  // allow clicks on non-clickables to hide the keyboard
  lv_obj_add_event_cb(cont, &WifiPanel::_handle_kb_input, LV_EVENT_CLICKED, this);

  spinner = lv_spinner_create(cont, 1000, 60);
  lv_obj_add_flag(spinner, LV_OBJ_FLAG_FLOATING);
  lv_obj_align(spinner, LV_ALIGN_CENTER, 0, 0);

  update_status();
  lv_obj_move_background(cont);

  wpa_event.register_callback("WifiPanel",
      [this](const std::string &event) { this->handle_wpa_event(event); });

  wpa_event.start();
}

WifiPanel::~WifiPanel() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

void WifiPanel::foreground() {
  spdlog::trace("wifi panel fg");
  lv_obj_move_foreground(cont);
  scan();
  powerui::overlay_open("Wi-Fi", [this]() {
    show_password_prompt(false);
    lv_obj_move_background(cont);
  });
}

void WifiPanel::scan() {
  if (networks.empty()) {
    lv_obj_clear_flag(spinner, LV_OBJ_FLAG_HIDDEN);  // only while there is nothing to show yet
    lv_obj_move_foreground(spinner);
  }
  wpa_event.send_command("SCAN");
}

bool WifiPanel::is_saved(const std::string &ssid) const {
  return list_networks.count(ssid) > 0;
}

bool WifiPanel::is_secure(const Network &network) {
  return network.flags.find("WPA") != std::string::npos || network.flags.find("WEP") != std::string::npos;
}

void WifiPanel::show_password_prompt(bool show) {
  if (show) {
    lv_obj_add_flag(ssid_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ip_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(signal_bars, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(signal_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(security_label, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(password_label, fmt::format("Password for {}", selected_network).c_str());
    lv_obj_clear_flag(password_cont, LV_OBJ_FLAG_HIDDEN);
    lv_event_send(password_input, LV_EVENT_FOCUSED, NULL);
  } else {
    lv_obj_add_flag(password_cont, LV_OBJ_FLAG_HIDDEN);
    lv_textarea_set_text(password_input, "");
    lv_keyboard_set_textarea(kb, NULL);
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ssid_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ip_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(signal_bars, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(signal_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(security_label, LV_OBJ_FLAG_HIDDEN);
  }
}

// Rows of the network list; the connected network comes first, then by signal.
void WifiPanel::rebuild_list() {
  std::string selected_ssid = selected >= 0 && selected < static_cast<int>(networks.size()) ? networks[selected].ssid : cur_network;
  std::stable_sort(networks.begin(), networks.end(), [this](const Network &a, const Network &b) {
    const bool a_connected = a.ssid == cur_network;
    const bool b_connected = b.ssid == cur_network;
    if (a_connected != b_connected) {
      return a_connected;
    }
    return a.signal > b.signal;
  });
  selected = -1;
  for (size_t i = 0; i < networks.size(); ++i) {
    if (networks[i].ssid == selected_ssid) {
      selected = static_cast<int>(i);
    }
  }
  if (selected < 0 && !networks.empty()) {
    selected = 0;
  }

  lv_obj_clean(list);
  for (size_t i = 0; i < networks.size(); ++i) {
    const Network &network = networks[i];
    const bool connected = network.ssid == cur_network;
    lv_obj_t *row = lv_obj_create(list);
    lv_obj_set_size(row, LV_PCT(100), px(58));
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_bg_color(row, lv_color_hex(COLOR_SECONDARY), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(row, lv_color_hex(COLOR_SECONDARY), LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_set_style_border_width(row, 1, LV_PART_MAIN);
    lv_obj_set_style_border_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_color(row, lv_color_hex(COLOR_ACCENT), LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_set_style_border_opa(row, LV_OPA_50, LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_set_style_radius(row, px(10), LV_PART_MAIN);
    lv_obj_set_style_pad_hor(row, px(14), LV_PART_MAIN);
    lv_obj_set_style_pad_ver(row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_column(row, px(14), LV_PART_MAIN);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    if (static_cast<int>(i) == selected) {
      lv_obj_add_state(row, LV_STATE_CHECKED);
    }

    lv_obj_t *bars = make_bars(row);
    fill_bars(bars, signal_level(network.signal), connected ? lv_color_hex(COLOR_ACCENT) : lv_color_hex(COLOR_MUTED));

    lv_obj_t *name = label(row, network.ssid.c_str(), &lv_font_montserrat_16, connected ? lv_color_hex(COLOR_ACCENT) : lv_color_hex(COLOR_FG));
    lv_obj_set_flex_grow(name, 1);
    lv_label_set_long_mode(name, LV_LABEL_LONG_CLIP);

    if (connected) {
      lv_obj_t *mark = label(row, LV_SYMBOL_OK "  Connected", &lv_font_montserrat_12, lv_color_hex(COLOR_ACCENT));
      (void)mark;
    } else if (is_secure(network)) {
      icon(row, &ui_icon_lock, 16, lv_color_hex(COLOR_MUTED));
    }
    lv_obj_add_event_cb(row, &WifiPanel::_handle_row, LV_EVENT_CLICKED, this);
  }
  lv_obj_scroll_to_y(list, 0, LV_ANIM_OFF);
}

// Status card and action buttons for the selected network.
void WifiPanel::update_status() {
  const bool has_selection = selected >= 0 && selected < static_cast<int>(networks.size());
  if (!has_selection) {
    lv_label_set_text(ssid_label, "Select a network");
    lv_label_set_text(ip_label, "");
    lv_label_set_text(signal_label, "");
    lv_label_set_text(security_label, "");
    fill_bars(signal_bars, 0, lv_color_hex(COLOR_MUTED));
    badge_set(status_badge, "Not connected", lv_color_hex(COLOR_MUTED));
    lv_label_set_text(connect_label, "Connect to network");
    lv_obj_add_state(connect_btn, LV_STATE_DISABLED);
    lv_obj_set_style_opa(connect_btn, LV_OPA_50, 0);
    lv_obj_add_flag(forget_btn, LV_OBJ_FLAG_HIDDEN);
    return;
  }

  const Network &network = networks[selected];
  const bool connected = network.ssid == cur_network;
  const bool saved = is_saved(network.ssid);
  lv_label_set_text(ssid_label, network.ssid.c_str());
  if (connected) {
    const std::string ip = KUtils::interface_ip(KUtils::get_wifi_interface());
    lv_label_set_text(ip_label, ip.c_str());
    badge_set(status_badge, "Connected", lv_color_hex(COLOR_ACCENT));
  } else {
    lv_label_set_text(ip_label, "");
    badge_set(status_badge, saved ? "Saved" : "Not connected", lv_color_hex(COLOR_MUTED));
  }
  const int level = signal_level(network.signal);
  fill_bars(signal_bars, level, connected ? lv_color_hex(COLOR_ACCENT) : lv_color_hex(COLOR_MUTED));
  lv_label_set_text(signal_label, signal_text(level));

  std::string security = "Open";
  for (const char *kind : {"WPA3", "WPA2", "WPA", "WEP"}) {
    if (network.flags.find(kind) != std::string::npos) {
      security = kind;
      break;
    }
  }
  lv_label_set_text(security_label, fmt::format("{} - {}", security, network.frequency >= 4900 ? "5 GHz" : "2.4 GHz").c_str());

  lv_label_set_text(connect_label, connected ? "Connected" : (saved ? "Connect" : "Connect to network"));
  if (connected) {
    lv_obj_add_state(connect_btn, LV_STATE_DISABLED);
    lv_obj_set_style_opa(connect_btn, LV_OPA_50, 0);
  } else {
    lv_obj_clear_state(connect_btn, LV_STATE_DISABLED);
    lv_obj_set_style_opa(connect_btn, LV_OPA_COVER, 0);
  }
  if (saved) {
    lv_obj_clear_flag(forget_btn, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(forget_btn, LV_OBJ_FLAG_HIDDEN);
  }
}

void WifiPanel::handle_row(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
    return;
  }
  lv_obj_t *row = lv_event_get_current_target(e);
  const int index = static_cast<int>(lv_obj_get_index(row));
  if (index < 0 || index >= static_cast<int>(networks.size())) {
    return;
  }
  selected = index;
  selected_network = networks[index].ssid;
  const uint32_t count = lv_obj_get_child_cnt(list);
  for (uint32_t i = 0; i < count; ++i) {
    lv_obj_t *item = lv_obj_get_child(list, i);
    if (static_cast<int>(i) == index) {
      lv_obj_add_state(item, LV_STATE_CHECKED);
    } else {
      lv_obj_clear_state(item, LV_STATE_CHECKED);
    }
  }
  show_password_prompt(false);
  update_status();
}

void WifiPanel::handle_action(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
    return;
  }
  lv_obj_t *button = lv_event_get_current_target(e);

  if (button == scan_btn || button == refresh_btn) {
    scan();
    return;
  }

  if (selected < 0 || selected >= static_cast<int>(networks.size())) {
    return;
  }
  const Network &network = networks[selected];
  selected_network = network.ssid;

  if (button == connect_btn) {
    if (network.ssid == cur_network) {
      return;
    }
    const auto saved = list_networks.find(network.ssid);
    if (saved != list_networks.end()) {
      wpa_event.send_command(fmt::format("SELECT_NETWORK {}", saved->second));
      wpa_event.send_command("SAVE_CONFIG");
    } else if (!is_secure(network)) {
      connect(NULL);
    } else {
      show_password_prompt(true);
    }
  } else if (button == forget_btn) {
    const auto saved = list_networks.find(network.ssid);
    if (saved != list_networks.end()) {
      wpa_event.send_command(fmt::format("REMOVE_NETWORK {}", saved->second));
      wpa_event.send_command("SAVE_CONFIG");
      find_current_network();
      rebuild_list();
      update_status();
    }
  }
}

void WifiPanel::handle_wpa_event(const std::string &event) {
  if (event.rfind("<3>CTRL-EVENT-SCAN-RESULTS", 0) == 0) {
    spdlog::trace("got scan result event");
    std::istringstream f(wpa_event.send_command("SCAN_RESULTS"));
    std::string line;
    std::map<std::string, Network> best;

    find_current_network();
    while (std::getline(f, line)) {
      if (line.rfind("bss", 0) == 0) {
        continue;
      }
      auto wifi_parts = KUtils::split(line, '\t');
      if (wifi_parts.size() != 5 || wifi_parts[4].empty()) {
        continue;
      }
      Network network;
      try {
        network.frequency = std::stoi(wifi_parts[1]);
        network.signal = std::stoi(wifi_parts[2]);
      } catch (const std::exception &) {
        continue;
      }
      network.flags = wifi_parts[3];
      network.ssid = wifi_parts[4];
      auto existing = best.find(network.ssid);
      if (existing == best.end() || existing->second.signal < network.signal) {
        best[network.ssid] = network;
      }
    }

    std::lock_guard<std::mutex> lock(lv_lock);
    networks.clear();
    for (const auto &entry : best) {
      networks.push_back(entry.second);
    }
    rebuild_list();
    update_status();
    lv_obj_add_flag(spinner, LV_OBJ_FLAG_HIDDEN);
  } else if (event.rfind("<3>CTRL-EVENT-CONNECTED", 0) == 0) {
    if (find_current_network()) {
      std::lock_guard<std::mutex> lock(lv_lock);
      rebuild_list();
      update_status();
      lv_obj_add_flag(spinner, LV_OBJ_FLAG_HIDDEN);
    }
  }
}

void WifiPanel::handle_kb_input(lv_event_t *e)
{
  const lv_event_code_t code = lv_event_get_code(e);

  if (code == LV_EVENT_FOCUSED) {
    lv_keyboard_set_textarea(kb, password_input);
    lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(kb);
  } else if (code == LV_EVENT_DEFOCUSED) {
    lv_keyboard_set_textarea(kb, NULL);
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
  } else if (code == LV_EVENT_READY) {
    const char *password = lv_textarea_get_text(password_input);
    if (password == NULL || password[0] == 0) {
      return;
    }

    // add network, set password, save wpa
    connect(password);
    show_password_prompt(false);
    lv_label_set_text(ssid_label, fmt::format("Connecting to {} ...", selected_network).c_str());
  } else if (code == LV_EVENT_CLICKED) {
    lv_obj_t *target = lv_event_get_target(e);
    if (target != kb && target != password_input && !lv_obj_has_flag(kb, LV_OBJ_FLAG_HIDDEN)) {
      lv_event_send(password_input, LV_EVENT_DEFOCUSED, NULL);
    }
  }
}

// password == NULL connects to an open network.
void WifiPanel::connect(const char *password) {
  std::string nid = wpa_event.send_command("ADD_NETWORK");
  spdlog::trace("add_nework {}", nid);
  if (nid.length() > 0) {
    wpa_event.send_command(fmt::format("SET_NETWORK {} ssid {:?}", nid, selected_network));
    if (password != NULL) {
      wpa_event.send_command(fmt::format("SET_NETWORK {} psk {:?}", nid, password));
    } else {
      wpa_event.send_command(fmt::format("SET_NETWORK {} key_mgmt NONE", nid));
    }
    wpa_event.send_command(fmt::format("ENABLE_NETWORK {}", nid));
    wpa_event.send_command(fmt::format("SELECT_NETWORK {}", nid));
    wpa_event.send_command("SAVE_CONFIG");
  }
}

bool WifiPanel::find_current_network() {
  list_networks.clear();
  std::string nets = wpa_event.send_command("LIST_NETWORKS");
  spdlog::trace("nets = {}", nets);
  std::istringstream f(nets);
  std::string line;
  bool found = false;
  cur_network.clear();
  while (std::getline(f, line)) {
    auto wifi_parts = KUtils::split(line, '\t');
    if (wifi_parts.size() == 4 && line.find("[CURRENT]") != std::string::npos) {
      cur_network = wifi_parts[1];
      found = true;
    }

    if (wifi_parts.size() > 1 && wifi_parts[0] != "network id") {
      list_networks[wifi_parts[1]] = wifi_parts[0];
    }
  }

  return found;
}
