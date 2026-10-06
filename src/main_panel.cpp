#include "main_panel.h"
#include "powerui.h"
#include "state.h"
#include "lvgl/lvgl.h"
#include "spdlog/spdlog.h"
#include "utils.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <experimental/filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace powerui;

LV_IMG_DECLARE(ui_icon_filament);
LV_IMG_DECLARE(light_img);
LV_IMG_DECLARE(move);
LV_IMG_DECLARE(extruder);
LV_IMG_DECLARE(bed);
LV_IMG_DECLARE(fan);
LV_IMG_DECLARE(heater);
LV_IMG_DECLARE(chamber);
LV_IMG_DECLARE(ui_logo);
LV_IMG_DECLARE(ui_icon_house);
LV_IMG_DECLARE(ui_icon_sliders);
LV_IMG_DECLARE(ui_icon_folder);
LV_IMG_DECLARE(ui_icon_settings);

#define CONSOLE_SYMBOL "\xF3\xB0\x86\x8D"
#define TUNE_SYMBOL "\xF3\xB1\x95\x82"
#define HOME_SYMBOL "\xF3\xB0\x8B\x9C"
#define SETTING_SYMBOL "\xF3\xB0\x92\x93"

namespace {
// Filament sensors reported by Klipper on the K1C; the factory one (nozzle MCU) is preferred.
const char *FILAMENT_SENSORS[] = {"filament_switch_sensor filament_sensor_2",
                                  "filament_switch_sensor filament_sensor"};
} // namespace

MainPanel::MainPanel(KWebSocketClient &websocket,
		     std::mutex &lock,
		     SpoolmanPanel &sm)
  : NotifyConsumer(lock)
  , ws(websocket)
  , homing_panel(ws, lock)
  , fan_panel(ws, lock)
  , led_panel(ws, lock)
  , tabview(lv_tabview_create(lv_scr_act(), LV_DIR_LEFT, px(64)))
  , nav_highlight(NULL)
  , nav_icons{NULL, NULL, NULL, NULL}
  , nav_tile_top{0, 0, 0, 0}
  , nav_tile_x(0)
  , main_tab(lv_tabview_add_tab(tabview, HOME_SYMBOL))
  , printertune_tab(lv_tabview_add_tab(tabview, TUNE_SYMBOL))
  , files_tab(lv_tabview_add_tab(tabview, CONSOLE_SYMBOL))
  , console_page(lv_obj_create(lv_scr_act()))
  , console_panel(ws, lock, console_page)
  , setting_tab(lv_tabview_add_tab(tabview, SETTING_SYMBOL))
  , setting_panel(websocket, lock, setting_tab, sm)
  , title_bar(lv_obj_create(lv_scr_act()))
  , title_label(lv_label_create(title_bar))
  , time_label(lv_label_create(title_bar))
  , logo(lv_img_create(title_bar))
  , title_label_bold(NULL)
  , back_pill(NULL)
  , clock_timer(NULL)
  , network_timer(NULL)
  , main_cont(lv_obj_create(main_tab))
  , print_status_panel(websocket, lock, main_cont)
  , exclude_object_panel(websocket, lock)
  , cfs_panel(websocket, lock)
  , print_panel(ws, lock, print_status_panel, files_tab)
  , printertune_panel(ws, lock, printertune_tab, print_status_panel.get_finetune_panel())
  , numpad(Numpad(main_cont))
  , extruder_panel(ws, lock, numpad, sm)
  , prompt_panel(websocket, lock, main_cont)
  , spoolman_panel(sm)
  , chart_card(NULL)
  , chart_toggle(NULL)
  , chart_badge(NULL)
  , temp_chart(lv_chart_create(main_cont))
  , print_view(false)
{
    const lv_color_t screen_background = lv_color_hex(COLOR_BG);

    // Title bar: logo (Home) or tab title, status icons and clock.
    // The active screen reserves the sidebar column and the title bar: every panel that is a direct child of the
    // screen (Filament, Homing, Print Status...) is placed and sized inside the remaining area, so the sidebar
    // and the title bar stay visible and usable on every screen. The title bar and the tab view cancel the
    // insets with negative offsets and absolute sizes.
    lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_left(lv_scr_act(), px(64), LV_PART_MAIN);
    lv_obj_set_style_pad_top(lv_scr_act(), px(40), LV_PART_MAIN);

    lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(title_bar, px(800), px(40));
    lv_obj_set_pos(title_bar, -px(64), -px(40));
    lv_obj_set_style_pad_all(title_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(title_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(title_bar, lv_color_hex(COLOR_CARD), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(title_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(title_bar, 1, LV_PART_MAIN);
    lv_obj_set_style_border_side(title_bar, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
    lv_obj_set_style_border_color(title_bar, lv_color_hex(COLOR_WHITE), LV_PART_MAIN);
    lv_obj_set_style_border_opa(title_bar, LV_OPA_10, LV_PART_MAIN);

    lv_img_set_src(logo, &ui_logo);
    lv_obj_clear_flag(logo, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(logo, LV_ALIGN_LEFT_MID, px(16), 0);

    // Title: centered and bold (Montserrat has no bold here, so it is drawn twice, one pixel apart).
    title_label_bold = lv_label_create(title_bar);
    for (lv_obj_t *l : {title_label, title_label_bold}) {
      lv_obj_set_width(l, LV_SIZE_CONTENT);
      lv_obj_set_style_text_color(l, lv_color_hex(COLOR_FG), LV_PART_MAIN);
      lv_obj_set_style_text_font(l, &lv_font_montserrat_20, LV_PART_MAIN);
      lv_obj_add_flag(l, LV_OBJ_FLAG_HIDDEN);
    }

    // Back button of the overlay panels, on the left of the title bar.
    back_pill = plain(title_bar);
    lv_obj_add_flag(back_pill, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(back_pill, px(96), px(32));
    lv_obj_align(back_pill, LV_ALIGN_LEFT_MID, px(8), 0);
    lv_obj_set_style_radius(back_pill, px(8), LV_PART_MAIN);
    lv_obj_set_style_bg_color(back_pill, lv_color_hex(powerui::COLOR_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(back_pill, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(back_pill, lv_color_hex(powerui::COLOR_PRIMARY_PRESSED), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(back_pill, 0, LV_PART_MAIN);
    lv_obj_set_style_border_color(back_pill, lv_color_hex(COLOR_WHITE), LV_PART_MAIN);
    lv_obj_set_style_border_opa(back_pill, LV_OPA_10, LV_PART_MAIN);
    lv_obj_t *back_text = label(back_pill, LV_SYMBOL_LEFT "  Back", &lv_font_montserrat_16, lv_color_hex(COLOR_WHITE));
    lv_obj_center(back_text);
    lv_obj_add_event_cb(back_pill, &MainPanel::_handle_back_click_cb, LV_EVENT_CLICKED, this);
    lv_obj_add_flag(back_pill, LV_OBJ_FLAG_HIDDEN);

    powerui::set_overlay_handlers([this](const std::string &title, std::function<void()> back) {
      push_overlay(title, back);
    });

    lv_obj_set_width(time_label, LV_SIZE_CONTENT);
    lv_obj_set_style_text_color(time_label, lv_color_hex(COLOR_FG), LV_PART_MAIN);
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_align(time_label, LV_ALIGN_RIGHT_MID, -px(16), 0);
    update_clock();
    clock_timer = lv_timer_create(&MainPanel::_update_clock_cb, 1000, this);
    lv_obj_update_layout(title_bar);
    status_icons = std::make_unique<StatusIcons>(title_bar, time_label);
    status_icons->set_camera_handler([this]() { toggle_camera(); });

    lv_obj_set_style_bg_color(lv_scr_act(), screen_background, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(tabview, screen_background, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(tabview, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(lv_tabview_get_content(tabview), screen_background, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(lv_tabview_get_content(tabview), LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(main_tab, screen_background, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(main_tab, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_set_pos(tabview, -px(64), 0);
    lv_obj_set_width(tabview, px(800));
    lv_obj_set_height(tabview, lv_obj_get_height(lv_scr_act()) - px(40));

    // The console lives in a full-screen page below the title bar; Settings opens it.
    lv_obj_clear_flag(console_page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(console_page, 0, 0);
    lv_obj_set_size(console_page, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_pad_all(console_page, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(console_page, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(console_page, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(console_page, screen_background, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(console_page, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_add_flag(console_page, LV_OBJ_FLAG_HIDDEN);
    printertune_panel.set_console_callback([this]() { open_console(); });

    ws.register_notify_update(this);
    setting_panel.set_cfs_opener([this]() { cfs_panel.foreground(); });
    extruder_panel.set_cfs_opener([this]() { cfs_panel.foreground(); });
    led_panel.set_state_callback([this](bool active) {
      set_quick_active(led_btn, active, active ? "LED On" : "LED Off");
    });
  print_status_panel.set_back_home_callback([this]() {
    print_panel.background();
    lv_tabview_set_act(tabview, 0, LV_ANIM_OFF);
    update_header();
  });
}

MainPanel::~MainPanel() {
  if (clock_timer != NULL) {
    lv_timer_del(clock_timer);
    clock_timer = NULL;
  }

  if (network_timer != NULL) {
    lv_timer_del(network_timer);
    network_timer = NULL;
  }

  if (tabview != NULL) {
    lv_obj_del(tabview);
    tabview = NULL;
  }

  sensors.clear();
}

void MainPanel::subscribe() {
  spdlog::trace("main panel subscribing");
  ws.send_jsonrpc("printer.gcode.help", [this](json &d) { console_panel.handle_macros(d); });
  print_panel.subscribe();
}

PrinterTunePanel& MainPanel::get_tune_panel() {
  return printertune_panel;
}

void MainPanel::open_files() {
  lv_tabview_set_act(tabview, 2, LV_ANIM_OFF);
  print_panel.foreground();
  update_header();
}

void MainPanel::open_console() {
  lv_obj_clear_flag(console_page, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(console_page);
  lv_obj_move_foreground(title_bar);
  push_overlay("Console", [this]() { lv_obj_add_flag(console_page, LV_OBJ_FLAG_HIDDEN); });
}

void MainPanel::update_filament_state(json &root, const std::string &prefix) {
  for (const char *name : FILAMENT_SENSORS) {
    auto v = root[json::json_pointer(fmt::format("{}/{}/filament_detected", prefix, name))];
    if (v.is_boolean()) {
      filament_state[name] = v.template get<bool>();
    }
  }

  bool known = false;
  bool detected = false;
  for (const char *name : FILAMENT_SENSORS) {
    auto it = filament_state.find(name);
    if (it != filament_state.end()) {
      known = true;
      detected = it->second;
      break;
    }
  }
  status_icons->set_filament(known && detected);
}

namespace {
// True when something listens on a port of this printer (the camera service answers on its streaming port).
bool local_port_open(int port) {
  const int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    return false;
  }
  fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
  sockaddr_in address;
  memset(&address, 0, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_port = htons(static_cast<uint16_t>(port));
  inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
  bool open = false;
  const int rc = connect(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address));
  if (rc == 0) {
    open = true;
  } else if (errno == EINPROGRESS) {
    fd_set writable;
    FD_ZERO(&writable);
    FD_SET(fd, &writable);
    timeval wait = {0, 300000};
    if (select(fd + 1, NULL, &writable, NULL, &wait) > 0) {
      int error = 1;
      socklen_t length = sizeof(error);
      getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &length);
      open = (error == 0);
    }
  }
  close(fd);
  return open;
}

// Port of a stream URL like "http://192.168.0.189:8080/?action=stream"; `fallback` when it has none.
int stream_port(const std::string &url, int fallback) {
  const auto scheme = url.find("://");
  if (scheme == std::string::npos) {
    return fallback;
  }
  const auto colon = url.find(':', scheme + 3);
  const auto slash = url.find('/', scheme + 3);
  if (colon == std::string::npos || (slash != std::string::npos && colon > slash)) {
    return fallback;
  }
  const int port = std::atoi(url.c_str() + colon + 1);
  return port > 0 && port < 65536 ? port : fallback;
}
}  // namespace

void MainPanel::poll_network() {
  // Wifi: any non-loopback interface with a routable IPv4 address. Camera: its streaming port answers.
  ws.send_jsonrpc("machine.system_info", [this](json &d) {
    bool connected = false;
    auto networks = d["/result/system_info/network"_json_pointer];
    if (networks.is_object()) {
      for (auto &iface : networks.items()) {
        if (iface.key() == "lo" || !iface.value().is_object()) {
          continue;
        }
        auto addresses = iface.value().value("ip_addresses", json::array());
        for (auto &address : addresses) {
          if (address.value("family", std::string()) == "ipv4"
              && !address.value("is_link_local", false)) {
            connected = true;
          }
        }
      }
    }
    std::lock_guard<std::mutex> guard(lv_lock);
    status_icons->set_wifi(connected);
  });

  ws.send_jsonrpc("server.webcams.list", [this](json &d) {
    int port = 8080;  // the mjpg-streamer of the K1C
    auto webcams = d["/result/webcams"_json_pointer];
    if (webcams.is_array()) {
      for (auto &cam : webcams) {
        if (cam.value("enabled", false)) {
          port = stream_port(cam.value("stream_url", std::string()), port);
          break;
        }
      }
    }
    const bool running = local_port_open(port);  // before taking the LVGL lock: it can wait up to 300 ms
    std::lock_guard<std::mutex> guard(lv_lock);
    camera_running = running;
    status_icons->set_camera(running);
  });
}

void MainPanel::toggle_camera() {
  const bool turn_off = camera_running;
  auto print_state = State::get_instance()->get_data("/printer_state/print_stats/state"_json_pointer);
  bool printing = false;
  if (print_state.is_string()) {
    const std::string value = print_state.template get<std::string>();
    printing = value == "printing" || value == "paused";
  }
  if (turn_off && printing) {
    // A touch on a small icon must not cost the timelapse photos of a print by accident.
    powerui::confirm_dialog("Turn the camera off?", "The timelapse takes its photos with the camera.", "Turn off",
                            powerui::ActionKind::Destructive, [this]() { send_camera_macro(true); }, "Keep it on");
    return;
  }
  send_camera_macro(turn_off);
}

void MainPanel::send_camera_macro(bool turn_off) {
  const std::string macro = turn_off ? "CAMERA_OFF" : "CAMERA_ON";
  spdlog::info("camera: {}", macro);
  camera_running = !turn_off;
  status_icons->set_camera(camera_running);  // right away; the real state is read again a few seconds later
  ws.send_jsonrpc("printer.gcode.script", json{{"script", macro}}, [this, macro](json &j) {
    if (j.contains("error")) {
      spdlog::warn("camera macro {} failed: {}", macro, j["error"].dump());
      std::lock_guard<std::mutex> guard(lv_lock);
      // The macros come with the Power Macros of the CFS Power Script.
      powerui::confirm_dialog("Camera macros not found",
                              "Install the Power Macros again from the CFS Power Script (Install menu, option 6).", "OK",
                              powerui::ActionKind::Outline, []() {});
    }
  });
  lv_timer_t *check = lv_timer_create(&MainPanel::_camera_check_cb, 7000, this);
  lv_timer_set_repeat_count(check, 1);
}

void MainPanel::init(json &j) {
  std::lock_guard<std::mutex> lock(lv_lock);
  sync_klipper_macros(j);
  update_header();
  led_panel.refresh();

  for (const auto &el : sensors) {
    auto target_value = j[json::json_pointer(fmt::format("/result/status/{}/target", el.first))];
    if (!target_value.is_null()) {
      int target = target_value.template get<int>();
      el.second->update_target(target);
    }

    auto temp_value = j[json::json_pointer(fmt::format("/result/status/{}/temperature", el.first))];
    if (!temp_value.is_null()) {
      int value = temp_value.template get<int>();
      el.second->update_series(value);
      el.second->update_value(value);
    }
  }

  update_filament_state(j, "/result/status");
  if (j.contains("result") && j["result"].is_object() && j["result"].contains("status")
      && j["result"]["status"].is_object() && j["result"]["status"].contains("box")) {
    box_cache = j["result"]["status"]["box"];
  }
  update_cfs();
  poll_network();
  if (network_timer == NULL) {
    network_timer = lv_timer_create(&MainPanel::_poll_network_cb, 30000, this);
  }

  auto fans = State::get_instance()->get_display_fans();
  print_status_panel.init(fans);
  printertune_panel.init(j);
}

void MainPanel::consume(json &j) {
  std::lock_guard<std::mutex> lock(lv_lock);
  for (const auto &el : sensors) {
    auto target_value = j[json::json_pointer(fmt::format("/params/0/{}/target", el.first))];
    if (!target_value.is_null()) {
      int target = target_value.template get<int>();
      el.second->update_target(target);
    }

    auto temp_value = j[json::json_pointer(fmt::format("/params/0/{}/temperature", el.first))];
    if (!temp_value.is_null()) {
      int value = temp_value.template get<int>();
      el.second->update_series(value);
      el.second->update_value(value);
    }
  }

  update_filament_state(j, "/params/0");
  if (j.contains("params") && j["params"].is_array() && !j["params"].empty() && j["params"][0].is_object()
      && j["params"][0].contains("box")) {
    box_cache.merge_patch(j["params"][0]["box"]);
    update_cfs();
  }
}

static void scroll_begin_event(lv_event_t * e)
{
  /*Disable the scroll animations. Triggered when a tab button is clicked */
  if (lv_event_get_code(e) == LV_EVENT_SCROLL_BEGIN) {
    lv_anim_t * a = (lv_anim_t*)lv_event_get_param(e);
    if(a)  a->time = 0;
  }
}

void MainPanel::create_panel() {
  lv_obj_clear_flag(lv_tabview_get_content(tabview), LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(lv_tabview_get_content(tabview), scroll_begin_event, LV_EVENT_SCROLL_BEGIN, NULL);

  lv_obj_t * tab_btns = lv_tabview_get_tab_btns(tabview);
  lv_obj_add_event_cb(tab_btns, &MainPanel::_handle_tab_change_cb,
                      LV_EVENT_VALUE_CHANGED, this);
  lv_obj_add_event_cb(tabview, &MainPanel::_handle_tab_change_cb,
                      LV_EVENT_VALUE_CHANGED, this);
  lv_obj_add_event_cb(tab_btns, &MainPanel::_handle_tab_click_cb, LV_EVENT_CLICKED, this);

  // Sidebar: dark column; the tab buttons stay as invisible touch areas and the icons and the active
  // highlight are drawn on top of them.
  lv_obj_set_style_bg_color(tab_btns, lv_color_hex(COLOR_BG), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(tab_btns, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(tab_btns, LV_OPA_TRANSP, LV_PART_ITEMS);
  lv_obj_set_style_bg_opa(tab_btns, LV_OPA_TRANSP, LV_STATE_CHECKED | LV_PART_ITEMS);
  lv_obj_set_style_bg_opa(tab_btns, LV_OPA_TRANSP, LV_STATE_PRESSED | LV_PART_ITEMS);
  lv_obj_set_style_text_opa(tab_btns, LV_OPA_TRANSP, LV_PART_ITEMS);
  lv_obj_set_style_border_width(tab_btns, 0, LV_PART_ITEMS);
  lv_obj_set_style_border_width(tab_btns, 0, LV_STATE_CHECKED | LV_PART_ITEMS);
  lv_obj_set_style_border_width(tab_btns, 0, LV_STATE_PRESSED | LV_PART_ITEMS);
  lv_obj_set_style_outline_width(tab_btns, 0, LV_PART_ITEMS | LV_STATE_FOCUS_KEY | LV_STATE_FOCUS_KEY);
  lv_obj_set_style_shadow_width(tab_btns, 0, LV_PART_ITEMS);
  // Keep the button matrix at the full display height. A main border would
  // reduce LVGL's content height and make the four rows progressively drift.
  lv_obj_set_style_pad_all(tab_btns, 0, LV_PART_MAIN);
  lv_obj_set_style_border_width(tab_btns, 0, LV_PART_MAIN);
  lv_obj_set_style_border_side(tab_btns, 0, LV_PART_MAIN);
  lv_obj_update_layout(tab_btns);

  const lv_coord_t tab_btns_width = lv_obj_get_width(tab_btns);
  const lv_coord_t tab_btns_height = lv_obj_get_height(tab_btns);
  const lv_coord_t tile = px(52);
  const lv_coord_t gap = (tab_btns_height - px(16) - 4 * tile) / 4;
  nav_tile_x = (tab_btns_width - tile) / 2;

  lv_obj_t *edge = plain(tab_btns);
  lv_obj_set_size(edge, 1, tab_btns_height);
  lv_obj_set_pos(edge, tab_btns_width - 1, 0);
  lv_obj_set_style_bg_color(edge, lv_color_hex(COLOR_WHITE), 0);
  lv_obj_set_style_bg_opa(edge, LV_OPA_10, 0);

  nav_highlight = plain(tab_btns);
  lv_obj_set_size(nav_highlight, tile, tile);
  lv_obj_set_style_radius(nav_highlight, px(10), 0);
  lv_obj_set_style_bg_color(nav_highlight, lv_color_hex(COLOR_SECONDARY), 0);
  lv_obj_set_style_bg_opa(nav_highlight, LV_OPA_COVER, 0);

  const lv_img_dsc_t *nav_sources[4] = {&ui_icon_house, &ui_icon_sliders, &ui_icon_folder, &ui_icon_settings};
  for (int tab_index = 0; tab_index < 4; ++tab_index) {
    nav_tile_top[tab_index] = px(8) + gap / 2 + tab_index * (tile + gap);
    nav_icons[tab_index] = icon(tab_btns, nav_sources[tab_index], 32, lv_color_hex(COLOR_MUTED));
    lv_obj_set_pos(nav_icons[tab_index], nav_tile_x + (tile - px(32)) / 2, nav_tile_top[tab_index] + (tile - px(32)) / 2);
  }

  lv_obj_set_style_pad_all(main_tab, 0, 0);
  lv_obj_clear_flag(main_tab, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(files_tab, 0, 0);
  lv_obj_clear_flag(files_tab, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(printertune_tab, 0, 0);
  lv_obj_set_style_pad_all(setting_tab, 0, 0);

  update_nav_indicator();

  create_main(main_tab);

}

void MainPanel::handle_homing_cb(lv_event_t *event) {
  if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
    spdlog::trace("clicked homing");
    homing_panel.foreground();
  }
}

void MainPanel::handle_extrude_cb(lv_event_t *event) {
  if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
    spdlog::trace("clicked extruder");
    extruder_panel.foreground();
  }
}

void MainPanel::handle_fanpanel_cb(lv_event_t *event) {
  if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
    spdlog::trace("clicked fan panel");
    fan_panel.foreground();
  }
}

void MainPanel::handle_ledpanel_cb(lv_event_t *event) {
  if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
    spdlog::trace("toggling led");
    led_panel.toggle();
  }
}

void MainPanel::handle_tab_change_cb(lv_event_t *event) {
  if (lv_event_get_code(event) == LV_EVENT_VALUE_CHANGED) {
    update_header();
  }
}

// Touching the sidebar always brings the tab view back on top of any open panel.
void MainPanel::handle_tab_click_cb(lv_event_t *event) {
  if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
    lv_obj_add_flag(console_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(tabview);
    overlays.clear();
    update_header();
  }
}

void MainPanel::push_overlay(const std::string &title, std::function<void()> back) {
  if (!overlays.empty() && overlays.back().title == title) {
    overlays.back().back = back;
  } else {
    overlays.push_back({title, back});
  }
  update_header();
}

// Back button of the title bar: close the top overlay panel.
void MainPanel::handle_back_click_cb(lv_event_t *event) {
  if (lv_event_get_code(event) == LV_EVENT_CLICKED && !overlays.empty()) {
    Overlay top = overlays.back();
    overlays.pop_back();
    if (top.back) {
      top.back();
    }
    update_header();
  }
}

void MainPanel::handle_view_toggle_cb(lv_event_t *event) {
  if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
    set_home_view(lv_obj_get_index(lv_event_get_target(event)) == 0);
  }
}

void MainPanel::set_home_view(bool show_print) {
  print_view = show_print;
  MiniPrintStatus &print_card = print_status_panel.get_mini_print_status();
  if (show_print) {
    lv_obj_clear_flag(print_card.get_container(), LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(chart_card, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(print_card.get_container(), LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(chart_card, LV_OBJ_FLAG_HIDDEN);
  }
  view_toggle_set(chart_toggle, show_print);
  view_toggle_set(print_card.get_toggle(), show_print);
}

MainPanel::QuickButton MainPanel::create_quick_button(lv_obj_t *parent, int x, int y, int w, int h,
                                                      const lv_img_dsc_t *icon_src, const char *text,
                                                      lv_event_cb_t cb) {
  QuickButton button;
  button.btn = plain(parent);
  lv_obj_add_flag(button.btn, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_pos(button.btn, px(x), px(y));
  lv_obj_set_size(button.btn, px(w), px(h));
  lv_obj_set_style_radius(button.btn, px(10), 0);
  lv_obj_set_style_bg_color(button.btn, lv_color_hex(COLOR_CARD), 0);
  lv_obj_set_style_bg_opa(button.btn, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(button.btn, lv_color_hex(COLOR_SECONDARY), LV_STATE_PRESSED);
  lv_obj_set_style_border_width(button.btn, 1, 0);
  lv_obj_set_style_border_color(button.btn, lv_color_hex(COLOR_WHITE), 0);
  lv_obj_set_style_border_opa(button.btn, LV_OPA_10, 0);
  lv_obj_set_flex_flow(button.btn, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(button.btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(button.btn, px(5), 0);

  button.icon = icon(button.btn, icon_src, 22, lv_color_hex(COLOR_FG));
  button.label = label(button.btn, text, &lv_font_montserrat_14, lv_color_hex(COLOR_FG));
  lv_obj_add_event_cb(button.btn, cb, LV_EVENT_CLICKED, this);
  return button;
}

void MainPanel::set_quick_active(QuickButton &button, bool active, const char *text) {
  if (button.btn == NULL) {
    return;
  }
  lv_obj_set_style_bg_color(button.btn, lv_color_hex(active ? COLOR_SECONDARY : COLOR_CARD), 0);
  lv_obj_set_style_border_color(button.btn, active ? lv_color_hex(COLOR_ACCENT) : lv_color_hex(COLOR_WHITE), 0);
  lv_obj_set_style_border_opa(button.btn, active ? LV_OPA_40 : LV_OPA_10, 0);
  icon_set_color(button.icon, lv_color_hex(active ? COLOR_ACCENT : COLOR_FG));
  lv_label_set_text(button.label, text);
}

void MainPanel::create_chart_card(lv_obj_t *parent) {
  chart_card = card(parent, 12, 12, 408, 416);
  lv_obj_set_style_pad_all(chart_card, px(20), 0);

  const lv_color_t fg = lv_color_hex(COLOR_FG);
  const lv_color_t muted = lv_color_hex(COLOR_MUTED);

  lv_obj_t *title = label(chart_card, "Temperatures", &lv_font_montserrat_16, fg);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_t *subtitle = label(chart_card, "Last 5 minutes", &lv_font_montserrat_14, muted);
  lv_obj_align(subtitle, LV_ALIGN_TOP_LEFT, 0, px(23));

  chart_toggle = view_toggle(chart_card, &MainPanel::_handle_view_toggle_cb, this);
  lv_obj_align(chart_toggle, LV_ALIGN_TOP_RIGHT, 0, 0);
  chart_badge = badge(chart_card, "Idle", muted);
  lv_obj_update_layout(chart_toggle);
  lv_obj_align_to(chart_badge, chart_toggle, LV_ALIGN_OUT_LEFT_MID, -px(8), 0);

  // The chart is created by the constructor (series are attached by the temperature cards) and moved here.
  lv_obj_set_parent(temp_chart, chart_card);
  // The axis labels are drawn outside the chart object, in the 34 px column on its left.
  lv_obj_set_size(temp_chart, px(332), px(272));
  lv_obj_align(temp_chart, LV_ALIGN_TOP_LEFT, px(34), px(60));
  lv_obj_set_scrollbar_mode(temp_chart, LV_SCROLLBAR_MODE_OFF);
  lv_obj_clear_flag(temp_chart, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_opa(temp_chart, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(temp_chart, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_left(temp_chart, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_right(temp_chart, px(4), LV_PART_MAIN);
  lv_obj_set_style_pad_top(temp_chart, px(8), LV_PART_MAIN);
  lv_obj_set_style_pad_bottom(temp_chart, px(8), LV_PART_MAIN);
  lv_obj_set_style_line_color(temp_chart, lv_color_hex(COLOR_WHITE), LV_PART_MAIN);
  lv_obj_set_style_line_opa(temp_chart, LV_OPA_10, LV_PART_MAIN);
  lv_obj_set_style_line_width(temp_chart, 1, LV_PART_MAIN);
  lv_obj_set_style_line_width(temp_chart, 2, LV_PART_ITEMS);
  lv_obj_set_style_line_rounded(temp_chart, true, LV_PART_ITEMS);
  lv_obj_set_style_size(temp_chart, 0, LV_PART_INDICATOR);
  lv_obj_set_style_text_font(temp_chart, &lv_font_montserrat_12, LV_PART_TICKS);
  lv_obj_set_style_text_color(temp_chart, muted, LV_PART_TICKS);

  lv_chart_set_type(temp_chart, LV_CHART_TYPE_LINE);
  lv_chart_set_range(temp_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 300);
  lv_chart_set_axis_tick(temp_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 0, 6, 1, true, px(34));
  lv_chart_set_div_line_count(temp_chart, 4, 0);
  lv_chart_set_point_count(temp_chart, 5000);
  lv_chart_set_zoom_x(temp_chart, 8533);
  lv_obj_scroll_to_x(temp_chart, LV_COORD_MAX, LV_ANIM_OFF);

  // Legend.
  const char *names[3] = {"Extruder", "Bed", "Chamber"};
  const uint32_t colors[3] = {COLOR_EXTRUDER, COLOR_BED, COLOR_CHAMBER};
  const int offsets[3] = {0, 90, 144};
  for (int i = 0; i < 3; ++i) {
    lv_obj_t *dot = plain(chart_card);
    lv_obj_set_size(dot, px(4), px(14));
    lv_obj_set_style_radius(dot, px(2), 0);
    lv_obj_set_style_bg_color(dot, lv_color_hex(colors[i]), 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_align(dot, LV_ALIGN_TOP_LEFT, px(offsets[i]), px(352));
    lv_obj_t *name = label(chart_card, names[i], &lv_font_montserrat_12, muted);
    lv_obj_align(name, LV_ALIGN_TOP_LEFT, px(offsets[i] + 12), px(351));
  }
}

void MainPanel::create_main(lv_obj_t *parent)
{
    // The Home screen uses absolute positions on a 800x480 design grid (see PowerUI Home specification).
    lv_obj_remove_style_all(main_cont);
    lv_obj_clear_flag(main_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(main_cont, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(main_cont, 0, 0);
    lv_obj_set_size(main_cont, LV_PCT(100), LV_PCT(100));

    create_chart_card(main_cont);

    // Right column: CFS strip (shown while the CFS is connected, between the temperatures and the quick actions)
    // and quick actions. The positions here are the ones without the CFS; apply_home_layout() moves them.
    cfs_pill = card(main_cont, 432, 228, 292, 56);
    lv_obj_set_style_radius(cfs_pill, LV_RADIUS_CIRCLE, 0);
    lv_obj_t *cfs_title = label(cfs_pill, "CFS", &lv_font_montserrat_14, lv_color_hex(COLOR_MUTED));
    lv_obj_align(cfs_title, LV_ALIGN_LEFT_MID, px(22), 0);
    for (int i = 0; i < 4; ++i) {
      cfs_spools[i] = cfs_ui::spool_create(cfs_pill, 36, lv_color_hex(COLOR_CARD), i + 1);
      lv_obj_set_pos(cfs_spools[i], px(86 + i * 50), px(9));
    }
    lv_obj_add_flag(cfs_pill, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(cfs_pill, [](lv_event_t *e) {
      static_cast<MainPanel *>(e->user_data)->cfs_panel.foreground();
    }, LV_EVENT_CLICKED, this);
    lv_obj_add_flag(cfs_pill, LV_OBJ_FLAG_HIDDEN);

    homing_btn = create_quick_button(main_cont, 432, 264, 140, 76, &move, "Homing", &MainPanel::_handle_homing_cb);
    extrude_btn = create_quick_button(main_cont, 584, 264, 140, 76, &ui_icon_filament, "Filament", &MainPanel::_handle_extrude_cb);
    action_btn = create_quick_button(main_cont, 432, 352, 140, 76, &fan, "Fans", &MainPanel::_handle_fanpanel_cb);
    led_btn = create_quick_button(main_cont, 584, 352, 140, 76, &light_img, "LED Off", &MainPanel::_handle_ledpanel_cb);

    // Print card <-> temperature chart switch.
    MiniPrintStatus &print_card = print_status_panel.get_mini_print_status();
    for (int i = 0; i < 2; ++i) {
      lv_obj_add_event_cb(lv_obj_get_child(print_card.get_toggle(), i), &MainPanel::_handle_view_toggle_cb,
                          LV_EVENT_CLICKED, this);
    }
    print_card.set_state_callback([this](bool active) { set_home_view(active); });
    print_card.set_status_callback([this](const char *text, lv_color_t dot) {
      badge_set(chart_badge, text, dot);
      lv_obj_update_layout(chart_badge);
      lv_obj_align_to(chart_badge, chart_toggle, LV_ALIGN_OUT_LEFT_MID, -px(8), 0);
    });
    // "Exclude objects" chip of the print card: opens the Exclude Object screen when the print has labeled objects.
    print_card.set_exclude_action([this]() { exclude_object_panel.foreground(); });
    exclude_object_panel.set_availability_callback([&print_card](bool available) { print_card.set_exclude_available(available); });
    // "Exclude objects" button of Print Status: needs two or more labeled objects.
    print_status_panel.set_exclude_action([this]() { exclude_object_panel.foreground(); });
    exclude_object_panel.set_count_callback([this](size_t count) { print_status_panel.set_exclude_count(count); });
    set_home_view(print_card.is_active());
}

void MainPanel::set_title(const char *text) {
  lv_label_set_text(title_label, text);
  lv_label_set_text(title_label_bold, text);
  lv_obj_align(title_label, LV_ALIGN_CENTER, 0, 0);
  lv_obj_align(title_label_bold, LV_ALIGN_CENTER, 1, 0);
  lv_obj_clear_flag(title_label, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(title_label_bold, LV_OBJ_FLAG_HIDDEN);
}

// Home shows the logo; every other screen shows its title (bold, centered) and overlay panels add a Back button.
void MainPanel::update_header() {
  const char *title = NULL;
  switch (lv_tabview_get_tab_act(tabview)) {
    case 1:
      title = "Calibrations";
      break;
    case 2:
      title = "Files";
      break;
    case 3:
      title = "Settings";
      break;
    default:
      break;
  }

  if (!overlays.empty()) {
    lv_obj_add_flag(logo, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(back_pill, LV_OBJ_FLAG_HIDDEN);
    set_title(overlays.back().title.c_str());
  } else if (title != NULL) {
    lv_obj_add_flag(logo, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(back_pill, LV_OBJ_FLAG_HIDDEN);
    set_title(title);
  } else {
    lv_obj_clear_flag(logo, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(back_pill, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(title_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(title_label_bold, LV_OBJ_FLAG_HIDDEN);
  }
  update_nav_indicator();
}

void MainPanel::update_nav_indicator() {
  if (tabview == NULL || nav_highlight == NULL) {
    return;
  }

  const uint32_t active_tab = lv_tabview_get_tab_act(tabview);
  if (active_tab < 4) {
    lv_obj_set_pos(nav_highlight, nav_tile_x, nav_tile_top[active_tab]);
  }
  for (uint32_t tab_index = 0; tab_index < 4; ++tab_index) {
    if (nav_icons[tab_index] != NULL) {
      icon_set_color(nav_icons[tab_index], lv_color_hex(tab_index == active_tab ? COLOR_FG : COLOR_MUTED));
    }
  }
}

void MainPanel::update_clock() {
  const std::time_t now = std::time(nullptr);
  const std::tm local_time = *std::localtime(&now);
  char time_text[6] = {};
  std::strftime(time_text, sizeof(time_text), "%H:%M", &local_time);
  lv_label_set_text(time_label, time_text);
}

void MainPanel::create_sensors(json &temp_sensors) {
  std::lock_guard<std::mutex> lock(lv_lock);
  sensors.clear();
  sensor_slot.clear();

  // The Home shows three temperature cards: extruder, bed and chamber, top to bottom. Slots are assigned by
  // sensor key (the json is sorted alphabetically); any other sensor takes a slot that is still free.
  const int card_y[3] = {12, 96, 180};
  std::vector<std::string> keys;
  std::map<std::string, int> slot_of;
  bool slot_used[3] = {false, false, false};
  for (auto &sensor : temp_sensors.items()) {
    const std::string key = sensor.key();
    int slot = -1;
    if (key == "extruder") {
      slot = 0;
    } else if (key == "heater_bed") {
      slot = 1;
    } else if (key == "chamber_temp" || key == "temperature_sensor chamber_temp") {
      slot = 2;
    }
    if (slot >= 0) {
      slot_of[key] = slot;
      slot_used[slot] = true;
    }
    keys.push_back(key);
  }
  for (const auto &key : keys) {
    if (slot_of.count(key) == 0) {
      for (int s = 0; s < 3; ++s) {
        if (!slot_used[s]) {
          slot_of[key] = s;
          slot_used[s] = true;
          break;
        }
      }
    }
  }

  for (auto &sensor : temp_sensors.items()) {
    std::string key = sensor.key();
    auto slot_it = slot_of.find(key);
    if (slot_it == slot_of.end()) {
      spdlog::debug("sensor {} has no free card on the Home screen", key);
      continue;
    }
    const int slot = slot_it->second;
    sensor_slot[key] = slot;
    bool controllable = sensor.value()["controllable"].template get<bool>();

    lv_color_t color_code = lv_color_hex(COLOR_MAT_ORANGE);
    const lv_img_dsc_t *sensor_img = &heater;
    std::string display_name = sensor.value()["display_name"].template get<std::string>();
    if (key == "extruder") {
      display_name = "Extruder";
      sensor_img = &extruder;
      color_code = lv_color_hex(COLOR_EXTRUDER);
    } else if (key == "heater_bed") {
      display_name = "Bed";
      sensor_img = &bed;
      color_code = lv_color_hex(COLOR_BED);
    } else if (key == "chamber_temp" || key == "temperature_sensor chamber_temp") {
      display_name = "Chamber";
      sensor_img = &chamber;
      color_code = lv_color_hex(COLOR_CHAMBER);
    }

    lv_chart_series_t *temp_series =
      lv_chart_add_series(temp_chart, color_code, LV_CHART_AXIS_PRIMARY_Y);

    sensors.insert({key, std::make_shared<TempCard>(ws, main_cont, 432, card_y[slot], 292, 72, sensor_img,
						    display_name.c_str(), color_code, controllable, numpad, key,
						    temp_chart, temp_series)});
  }
  apply_home_layout(home_with_cfs);
}

void MainPanel::place_quick(QuickButton &button, int x, int y, int w, int h) {
  if (button.btn == NULL) {
    return;
  }
  lv_obj_set_pos(button.btn, px(x), px(y));
  lv_obj_set_size(button.btn, px(w), px(h));
}

void MainPanel::apply_home_layout(bool with_cfs) {
  home_with_cfs = with_cfs;
  // 3 cards + 2 rows of buttons + (strip) fill the 416 px column: 60/60/56 with the CFS, 72/76 without it.
  const int card_h = with_cfs ? 60 : 72;
  for (const auto &el : sensors) {
    auto slot = sensor_slot.find(el.first);
    if (slot != sensor_slot.end()) {
      el.second->set_geometry(12 + slot->second * (card_h + 12), card_h);
    }
  }
  const int quick_h = with_cfs ? 60 : 76;
  const int row1 = with_cfs ? 296 : 264;
  const int row2 = with_cfs ? 368 : 352;
  place_quick(homing_btn, 432, row1, 140, quick_h);
  place_quick(extrude_btn, 584, row1, 140, quick_h);
  place_quick(action_btn, 432, row2, 140, quick_h);
  place_quick(led_btn, 584, row2, 140, quick_h);
  if (cfs_pill != NULL) {
    if (with_cfs) {
      lv_obj_clear_flag(cfs_pill, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(cfs_pill, LV_OBJ_FLAG_HIDDEN);
    }
  }
}

void MainPanel::update_cfs() {
  cfs::State next = cfs::parse(box_cache);
  const bool connection_changed = next.connected != cfs_state.connected || !home_layout_ready;
  const bool changed = !cfs::same(next, cfs_state);
  cfs_state = next;
  if (!changed && !connection_changed) {
    return;
  }
  for (int i = 0; i < 4; ++i) {
    cfs_ui::spool_set(cfs_spools[i], cfs_state.slots[i], cfs_state.active == i);
  }
  if (connection_changed) {
    apply_home_layout(cfs_state.connected);
    home_layout_ready = true;
  }
  cfs_panel.update(cfs_state);
  setting_panel.set_cfs_available(cfs_state.connected);
  extruder_panel.set_cfs_available(cfs_state.connected);
}

void MainPanel::create_fans(json &fans) {
  fan_panel.create_fans(fans);
}

void MainPanel::create_leds(json &leds) {
  led_panel.init(leds);
}

void MainPanel::enable_spoolman() {
  spoolman_panel.init();
  setting_panel.enable_spoolman();
  extruder_panel.enable_spoolman();
}

void MainPanel::sync_klipper_macros(json &printer_status) {
  namespace fs = std::experimental::filesystem;
  try {
    const fs::path source = fs::canonical("/proc/self/exe").parent_path() / "scripts";
    const std::string config_root = KUtils::get_root_path("config");
    if (config_root.empty() || !fs::is_directory(source)) {
      return;
    }
    const fs::path target = fs::path(config_root) / "PowerScreen";
    if (!fs::is_directory(target)) {
      return;  // PowerScreen is not installed in the Klipper config of this printer
    }

    auto read_all = [](const fs::path &path) {
      std::ifstream in(path.string(), std::ios::binary);
      return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    };
    auto copy_if_different = [&read_all](const fs::path &from, const fs::path &to) {
      std::error_code ec;
      if (fs::exists(to, ec) && read_all(from) == read_all(to)) {
        return false;
      }
      fs::copy_file(from, to, fs::copy_options::overwrite_existing, ec);
      if (ec) {
        spdlog::warn("could not copy {} to {}: {}", from.string(), to.string(), ec.message());
        return false;
      }
      return true;
    };

    bool macros_changed = false;
    for (const auto &entry : fs::directory_iterator(source)) {
      if (entry.path().extension() == ".cfg") {
        macros_changed = copy_if_different(entry.path(), target / entry.path().filename()) || macros_changed;
      }
    }
    std::error_code ec;
    fs::create_directories(target / "scripts", ec);
    for (const auto &entry : fs::directory_iterator(source)) {
      if (entry.path().extension() == ".py") {
        copy_if_different(entry.path(), target / "scripts" / entry.path().filename());
      }
    }

    if (!macros_changed) {
      return;
    }
    // Klipper only reads its macros at startup. Restart it only when we know the printer is idle.
    std::string state;
    auto from_status = printer_status["/result/status/print_stats/state"_json_pointer];
    if (from_status.is_string()) {
      state = from_status.template get<std::string>();
    } else {
      auto from_state = State::get_instance()->get_data("/printer_state/print_stats/state"_json_pointer);
      if (from_state.is_string()) {
        state = from_state.template get<std::string>();
      }
    }
    if (state == "standby" || state == "complete" || state == "cancelled" || state == "error") {
      spdlog::info("PowerScreen macros changed, restarting Klipper to load them");
      ws.send_jsonrpc("printer.restart");
    } else {
      spdlog::info("PowerScreen macros changed; Klipper restart postponed (print state: '{}')", state);
    }
  } catch (const std::exception &error) {
    spdlog::warn("macro sync failed: {}", error.what());
  }
}
