#include "main_panel.h"
#include "state.h"
#include "lvgl/lvgl.h"
#include "spdlog/spdlog.h"

#include <ctime>
#include <string>

LV_IMG_DECLARE(filament_img);
LV_IMG_DECLARE(light_img);
LV_IMG_DECLARE(move);
LV_IMG_DECLARE(print);
LV_IMG_DECLARE(extruder);
LV_IMG_DECLARE(bed);
LV_IMG_DECLARE(fan);
LV_IMG_DECLARE(heater);

LV_FONT_DECLARE(materialdesign_font_40);
#define CREALITY_GREEN 0x4CAF50
#define CONSOLE_SYMBOL "\xF3\xB0\x86\x8D"
#define TUNE_SYMBOL "\xF3\xB1\x95\x82"
#define HOME_SYMBOL "\xF3\xB0\x8B\x9C"
#define SETTING_SYMBOL "\xF3\xB0\x92\x93"

MainPanel::MainPanel(KWebSocketClient &websocket,
		     std::mutex &lock,
		     SpoolmanPanel &sm)
  : NotifyConsumer(lock)
  , ws(websocket)
  , homing_panel(ws, lock)
  , fan_panel(ws, lock)
  , led_panel(ws, lock)
  , tabview(lv_tabview_create(lv_scr_act(), LV_DIR_LEFT, 60))
  , main_tab(lv_tabview_add_tab(tabview, HOME_SYMBOL))
  , printertune_tab(lv_tabview_add_tab(tabview, TUNE_SYMBOL))
  , console_tab(lv_tabview_add_tab(tabview, CONSOLE_SYMBOL))
  , console_panel(ws, lock, console_tab)
  , setting_tab(lv_tabview_add_tab(tabview, SETTING_SYMBOL))
  , setting_panel(websocket, lock, setting_tab, sm)
  , title_bar(lv_obj_create(lv_scr_act()))
  , title_label(lv_label_create(title_bar))
  , time_label(lv_label_create(title_bar))
  , clock_timer(NULL)
  , main_cont(lv_obj_create(main_tab))
  , print_status_panel(websocket, lock, main_cont)
  , print_panel(ws, lock, print_status_panel)
  , printertune_panel(ws, lock, printertune_tab, print_status_panel.get_finetune_panel())
  , numpad(Numpad(main_cont))
  , extruder_panel(ws, lock, numpad, sm)
  , prompt_panel(websocket, lock, main_cont)
  , spoolman_panel(sm)
  , temp_cont(lv_obj_create(main_cont))
  , temp_chart(lv_chart_create(main_cont))
  , homing_btn(main_cont, &move, "Homing", &MainPanel::_handle_homing_cb, this)
  , extrude_btn(main_cont, &filament_img, "Filament", &MainPanel::_handle_extrude_cb, this)
  , action_btn(main_cont, &fan, "Fans", &MainPanel::_handle_fanpanel_cb, this)
  , led_btn(main_cont, &light_img, "LED", &MainPanel::_handle_ledpanel_cb, this)
  , print_btn(main_cont, &print, "Print", &MainPanel::_handle_print_cb, this)
{
    const lv_color_t button_grey = lv_color_hex(0x555555);
    const lv_color_t screen_background = lv_palette_darken(LV_PALETTE_GREY, 4);

    lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(title_bar, LV_PCT(100), 32);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_set_style_pad_all(title_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(title_bar, button_grey, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(title_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(title_bar, 0, LV_PART_MAIN);

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
    clock_timer = lv_timer_create(&MainPanel::_update_clock_cb, 1000, this);

    lv_obj_set_style_bg_color(lv_scr_act(), screen_background, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(tabview, screen_background, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(tabview, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(lv_tabview_get_content(tabview), screen_background, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(lv_tabview_get_content(tabview), LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(main_tab, screen_background, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(main_tab, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_set_pos(tabview, 0, 32);
    lv_obj_set_width(tabview, LV_PCT(100));
    lv_obj_set_height(tabview, lv_obj_get_height(lv_scr_act()) - 32);

    homing_btn.set_icon_color(lv_color_white());
    extrude_btn.set_icon_color(lv_color_white());
    action_btn.set_icon_color(lv_color_white());
    led_btn.set_icon_color(lv_color_white());
    action_btn.set_background_visible(false);
    led_btn.set_background_visible(false);

    lv_style_init(&style);
    lv_style_set_img_recolor_opa(&style, LV_OPA_30);
    lv_style_set_img_recolor(&style, lv_color_black());
    lv_style_set_border_width(&style, 0);
    lv_style_set_bg_color(&style, lv_palette_darken(LV_PALETTE_GREY, 4));
    ws.register_notify_update(this);
    led_panel.set_state_callback([this](bool active) {
      led_btn.set_active(active, lv_color_hex(CREALITY_GREEN));
    });
}

MainPanel::~MainPanel() {
  if (clock_timer != NULL) {
    lv_timer_del(clock_timer);
    clock_timer = NULL;
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

void MainPanel::init(json &j) {
  std::lock_guard<std::mutex> lock(lv_lock);
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
  const lv_color_t nav_selected_bg = lv_palette_darken(LV_PALETTE_GREY, 4);
  const lv_color_t nav_unselected_bg = lv_color_hex(0x555555);
  // The active tab blends with the Home background; inactive tabs use the
  // same gray as the title bar.
  lv_obj_set_style_bg_color(tab_btns, nav_unselected_bg, LV_PART_ITEMS);
  lv_obj_set_style_bg_opa(tab_btns, LV_OPA_COVER, LV_PART_ITEMS);
  lv_obj_set_style_bg_color(tab_btns, nav_selected_bg, LV_STATE_CHECKED | LV_PART_ITEMS);
  lv_obj_set_style_bg_opa(tab_btns, LV_OPA_COVER, LV_STATE_CHECKED | LV_PART_ITEMS);
  lv_obj_set_style_outline_width(tab_btns, 0, LV_PART_ITEMS | LV_STATE_FOCUS_KEY | LV_STATE_FOCUS_KEY);
  lv_obj_set_style_border_side(tab_btns, 0, LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_text_font(tab_btns, &materialdesign_font_40, LV_STATE_DEFAULT);
  // Keep the button matrix at the full display height. A main border would
  // reduce LVGL's content height and make the four rows progressively drift.
  lv_obj_set_style_pad_all(tab_btns, 0, LV_PART_MAIN);
  lv_obj_set_style_border_width(tab_btns, 0, LV_PART_MAIN);
  lv_obj_set_style_border_side(tab_btns, 0, LV_PART_MAIN);
  lv_obj_update_layout(tab_btns);

  // Keep the green edge as four independent segments so the active tab can
  // hide only its own segment.
  const lv_coord_t tab_btns_width = lv_obj_get_width(tab_btns);
  const lv_coord_t tab_btns_height = lv_obj_get_height(tab_btns);
  for (int tab_index = 0; tab_index < 4; ++tab_index) {
    const lv_coord_t segment_top = (tab_btns_height * tab_index) / 4;
    const lv_coord_t segment_bottom = (tab_btns_height * (tab_index + 1)) / 4;
    nav_indicators[tab_index] = lv_obj_create(tab_btns);
    lv_obj_remove_style_all(nav_indicators[tab_index]);
    lv_obj_set_size(nav_indicators[tab_index], 3, segment_bottom - segment_top);
    lv_obj_set_style_bg_color(nav_indicators[tab_index], lv_color_hex(CREALITY_GREEN), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(nav_indicators[tab_index], LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(nav_indicators[tab_index], LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(nav_indicators[tab_index], tab_btns_width - 3, segment_top);
  }

  for (int divider_index = 1; divider_index < 4; ++divider_index) {
    lv_obj_t *nav_divider_top = lv_obj_create(tab_btns);
    lv_obj_remove_style_all(nav_divider_top);
    lv_obj_set_width(nav_divider_top, LV_PCT(100));
    lv_obj_set_height(nav_divider_top, 1);
    lv_obj_set_style_bg_color(nav_divider_top, lv_color_hex(CREALITY_GREEN), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(nav_divider_top, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(nav_divider_top, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(nav_divider_top, 0, (lv_obj_get_height(tab_btns) * divider_index) / 4 - 2);

    lv_obj_t *nav_divider = lv_obj_create(tab_btns);
    lv_obj_remove_style_all(nav_divider);
    lv_obj_set_width(nav_divider, LV_PCT(100));
    lv_obj_set_height(nav_divider, 2);
    lv_obj_set_style_bg_color(nav_divider, lv_color_hex(CREALITY_GREEN), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(nav_divider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(nav_divider, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(nav_divider, 0, (lv_obj_get_height(tab_btns) * divider_index) / 4 - 1);
  }

  lv_obj_set_style_pad_all(main_tab, 0, 0);
  lv_obj_clear_flag(main_tab, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(console_tab, 0, 0);
  lv_obj_set_style_pad_all(printertune_tab, 0, 0);
  lv_obj_set_style_pad_all(setting_tab, 0, 0);

  update_nav_indicator();

  create_main(main_tab);
  
}

void MainPanel::handle_homing_cb(lv_event_t *event) {
  spdlog::trace("clicked homing1");
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

void MainPanel::handle_print_cb(lv_event_t *event) {
  if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
    spdlog::trace("clicked print");
    print_panel.foreground();
  }
}

void MainPanel::handle_tab_change_cb(lv_event_t *event) {
  if (lv_event_get_code(event) == LV_EVENT_VALUE_CHANGED) {
    update_header();
  }
}

void MainPanel::create_main(lv_obj_t * parent)
{
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);

    static lv_coord_t grid_main_row_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t grid_main_col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
      LV_GRID_TEMPLATE_LAST};

    lv_obj_clear_flag(main_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(main_cont, LV_PCT(100));
    lv_obj_set_height(main_cont, LV_SIZE_CONTENT);

    lv_obj_set_flex_grow(main_cont, 1);
    lv_obj_set_grid_dsc_array(main_cont, grid_main_col_dsc, grid_main_row_dsc);    

    lv_obj_set_grid_cell(homing_btn.get_button(), LV_GRID_ALIGN_CENTER, 2, 1, LV_GRID_ALIGN_CENTER, 0, 1);
    lv_obj_set_grid_cell(extrude_btn.get_button(), LV_GRID_ALIGN_CENTER, 3, 1, LV_GRID_ALIGN_CENTER, 0, 1);
    lv_obj_set_grid_cell(action_btn.get_button(), LV_GRID_ALIGN_CENTER, 2, 1, LV_GRID_ALIGN_CENTER, 1, 1);
    lv_obj_set_grid_cell(led_btn.get_button(), LV_GRID_ALIGN_CENTER, 3, 1, LV_GRID_ALIGN_CENTER, 1, 1);
    // The print control is a single wide button spanning the two lower action
    // columns, matching the icon-and-label controls used by the M600 prompt.
    lv_obj_set_grid_cell(print_btn.get_button(), LV_GRID_ALIGN_STRETCH, 2, 2,
                         LV_GRID_ALIGN_CENTER, 2, 1);
    lv_obj_set_height(print_btn.get_button(), 100);

    lv_obj_clear_flag(temp_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(temp_cont, LV_PCT(50), LV_PCT(50));
    lv_obj_set_style_pad_all(temp_cont, 8, 0);
    lv_obj_set_style_bg_opa(temp_cont, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(temp_cont, 0, LV_PART_MAIN);
    lv_obj_set_style_translate_y(temp_cont, -20, LV_PART_MAIN);

    lv_obj_set_flex_flow(temp_cont, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_grid_cell(temp_cont, LV_GRID_ALIGN_START, 0, 2, LV_GRID_ALIGN_CENTER, 0, 2);
    
    lv_obj_align(temp_chart, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_size(temp_chart, LV_PCT(45), LV_PCT(40));
    lv_obj_set_style_bg_opa(temp_chart, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(temp_chart, 0, LV_PART_MAIN);
    lv_obj_set_style_translate_y(temp_chart, -10, LV_PART_MAIN);
    lv_obj_set_style_size(temp_chart, 0, LV_PART_INDICATOR);

    lv_chart_set_range(temp_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 300);
    lv_obj_set_grid_cell(temp_chart, LV_GRID_ALIGN_END, 0, 2, LV_GRID_ALIGN_END, 2, 1);
    lv_chart_set_axis_tick(temp_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 0, 6, 5, true, 50);

    lv_chart_set_div_line_count(temp_chart, 3, 8);
    lv_chart_set_point_count(temp_chart, 5000);
    lv_chart_set_zoom_x(temp_chart, 5000);
    lv_obj_scroll_to_x(temp_chart, LV_COORD_MAX, LV_ANIM_OFF);
}

void MainPanel::update_header() {
  const char *title = "POWER SCREEN K1C";
  switch (lv_tabview_get_tab_act(tabview)) {
    case 1:
      title = "CALIBRATIONS";
      break;
    case 2:
      title = "CONSOLE";
      break;
    case 3:
      title = "SETTINGS";
      break;
    default:
      break;
  }
  lv_label_set_text(title_label, title);
  update_nav_indicator();
}

void MainPanel::update_nav_indicator() {
  if (tabview == NULL) {
    return;
  }

  const uint32_t active_tab = lv_tabview_get_tab_act(tabview);
  for (uint32_t tab_index = 0; tab_index < 4; ++tab_index) {
    if (nav_indicators[tab_index] == NULL) {
      continue;
    }

    if (tab_index == active_tab) {
      lv_obj_add_flag(nav_indicators[tab_index], LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_clear_flag(nav_indicators[tab_index], LV_OBJ_FLAG_HIDDEN);
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
  for (auto &sensor : temp_sensors.items()) {
    std::string key = sensor.key();
    bool controllable = sensor.value()["controllable"].template get<bool>();

    lv_color_t color_code = lv_palette_main(LV_PALETTE_ORANGE);
    if (!sensor.value()["color"].is_number()) {
      std::string color = sensor.value()["color"].template get<std::string>();
      if (color == "red") {
	color_code = lv_palette_main(LV_PALETTE_RED);
      } else if (color == "purple") {
	color_code = lv_palette_main(LV_PALETTE_PURPLE);
      } else if (color == "blue") {
	color_code = lv_palette_main(LV_PALETTE_BLUE);	
      }
    } else {
      color_code = lv_palette_main((lv_palette_t)sensor.value()["color"].template get<int>());
    }

    std::string display_name = sensor.value()["display_name"].template get<std::string>();

    const void* sensor_img = &heater;
    if (key == "extruder") {
      sensor_img = &extruder;
    } else if (key == "heater_bed") {
      sensor_img = &bed;
    }

    lv_chart_series_t *temp_series =
      lv_chart_add_series(temp_chart, color_code, LV_CHART_AXIS_PRIMARY_Y);

    sensors.insert({key, std::make_shared<SensorContainer>(ws, temp_cont, sensor_img, 150,
			   display_name.c_str(), color_code, controllable, false, numpad, key,
        		   temp_chart, temp_series)});
  }
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
