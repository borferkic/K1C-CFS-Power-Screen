#include "powerscreen.h"

#include "config.h"
#ifndef OS_ANDROID
  #include "lv_drivers/display/fbdev.h"
  #include "lv_drivers/indev/evdev.h"
  
  #include "spdlog/sinks/rotating_file_sink.h"
  #include "spdlog/sinks/stdout_sinks.h"

#else
  #include "spdlog/sinks/android_sink.h"
#endif

#include "printer_select_panel.h"
#include "spdlog/spdlog.h"
#include "state.h"
#include "powerui.h"

PowerScreen *PowerScreen::instance = NULL;
lv_style_t PowerScreen::style_container;
lv_style_t PowerScreen::style_imgbtn_default;
lv_style_t PowerScreen::style_imgbtn_pressed;
lv_style_t PowerScreen::style_imgbtn_disabled;
lv_theme_t PowerScreen::th_new;

#ifndef OS_ANDROID
lv_obj_t *PowerScreen::screen_saver = NULL;
#endif

KWebSocketClient PowerScreen::ws(NULL);

std::mutex PowerScreen::lv_lock;

PowerScreen::PowerScreen()
  : spoolman_panel(ws, lv_lock)
  , main_panel(ws, lv_lock, spoolman_panel)
  , init_panel(main_panel, main_panel.get_tune_panel().get_bedmesh_panel(), lv_lock)
{
  main_panel.create_panel();
}

PowerScreen *PowerScreen::get() {
  if (instance == NULL) {
    instance = new PowerScreen();
  }

  return instance;
}

PowerScreen *PowerScreen::init(std::function<void(lv_color_t, lv_color_t)> hal_init) {
  hlog_disable();

  // config
  Config *conf = Config::get_instance();
  const std::string ll_path = conf->df() + "log_level";
  auto ll = spdlog::level::from_str(
      conf->get_json("/printers").empty() 
      ? "debug" 
      : conf->get<std::string>(ll_path));

  // PowerUI has fixed colors: the accent is not selectable.
  auto primary_color = lv_color_hex(powerui::COLOR_PRIMARY);
  auto secondary_color = lv_color_hex(powerui::COLOR_LVGL_SECONDARY);

#ifndef OS_ANDROID
  auto console_sink = std::make_shared<spdlog::sinks::stdout_sink_mt>();
  auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
      conf->get<std::string>("/log_path"), 1048576 * 10, 3);
  spdlog::sinks_init_list log_sinks{console_sink, file_sink};

#else
  auto android_sink = std::make_shared<spdlog::sinks::android_sink_mt>();
  spdlog::sinks_init_list log_sinks{android_sink};
#endif  // OS_ANDROID

  auto klogger = std::make_shared<spdlog::logger>("powerscreen", log_sinks);
  spdlog::register_logger(klogger);

  spdlog::set_level(ll);
  spdlog::set_default_logger(klogger);
  klogger->flush_on(ll);

#ifdef POWERSCREEN_VERSION
  spdlog::info("PowerScreen Version: {}", POWERSCREEN_VERSION);
#endif  // POWERSCREEN_VERSION

  spdlog::info("DPI: {}", LV_DPI_DEF);
  /*LittlevGL init*/
  lv_init();

#if !defined(SIMULATOR) && !defined(OS_ANDROID)
  /*Linux frame buffer device init*/
  fbdev_init();
  fbdev_unblank();
#endif  // OS_ANDROID

  hal_init(primary_color, secondary_color);
  lv_png_init();
  lv_split_jpeg_init();

  lv_style_init(&style_container);
  lv_style_set_border_width(&style_container, 0);
  lv_style_set_radius(&style_container, 0);

//  lv_style_init(&style_imgbtn_default);
//  lv_style_set_img_recolor_opa(&style_imgbtn_default, LV_OPA_100);
//  lv_style_set_img_recolor(&style_imgbtn_default, lv_color_hex(powerui::COLOR_BLACK));

  lv_style_init(&style_imgbtn_pressed);
  lv_style_set_img_recolor_opa(&style_imgbtn_pressed, LV_OPA_100);
  lv_style_set_img_recolor(&style_imgbtn_pressed, primary_color);

  lv_style_init(&style_imgbtn_disabled);
  lv_style_set_img_recolor_opa(&style_imgbtn_disabled, LV_OPA_100);
  lv_style_set_img_recolor(&style_imgbtn_disabled, lv_color_hex(powerui::COLOR_MAT_GREY_DARK));

  /*Initia1ize the new theme from the current theme*/

  lv_theme_t *th_act = lv_disp_get_theme(NULL);
  th_new = *th_act;

  /*Set the parent theme and the style apply callback for the new theme*/
  lv_theme_set_parent(&th_new, th_act);
  lv_theme_set_apply_cb(&th_new, &PowerScreen::new_theme_apply_cb);

  /*Assign the new theme to the current display*/
  lv_disp_set_theme(NULL, &th_new);

  ws.register_notify_update(State::get_instance());

  PowerScreen *gs = PowerScreen::get();
  auto printers = conf->get_json("/printers");
  if (!printers.empty()) {
    // start initializing all powerscreen components
    std::string ws_url = fmt::format("ws://{}:{}/websocket",
                                     conf->get<std::string>(conf->df() + "moonraker_host"),
                                     conf->get<uint32_t>(conf->df() + "moonraker_port"));

    spdlog::info("connecting to printer at {}", ws_url);
    gs->connect_ws(ws_url);
  }

#ifndef OS_ANDROID
  screen_saver = lv_obj_create(lv_scr_act());

  // The screen reserves a sidebar and title bar inset; the screen saver must cover all of it.
  lv_obj_set_size(screen_saver, lv_disp_get_hor_res(NULL), lv_disp_get_ver_res(NULL));
  lv_obj_set_pos(screen_saver, -lv_obj_get_style_pad_left(lv_scr_act(), LV_PART_MAIN),
                 -lv_obj_get_style_pad_top(lv_scr_act(), LV_PART_MAIN));
  lv_obj_set_style_bg_opa(screen_saver, LV_OPA_100, 0);
  lv_obj_move_background(screen_saver);

  lv_obj_t *main_screen = lv_disp_get_scr_act(NULL);
  auto touch_calibrated = conf->get_json("/touch_calibrated");
  if (!touch_calibrated.is_null()) {
    auto is_calibrated = touch_calibrated.template get<bool>();
    if (is_calibrated) {
      auto calibration_coeff = conf->get_json("/touch_calibration_coeff");
      if (calibration_coeff.is_null()) {
        lv_tc_register_coeff_save_cb(&PowerScreen::save_calibration_coeff);
        lv_obj_t *touch_calibrate_scr = lv_tc_screen_create();

        lv_disp_load_scr(touch_calibrate_scr);

        lv_tc_screen_start(touch_calibrate_scr);
        lv_obj_add_event_cb(touch_calibrate_scr, &PowerScreen::handle_calibrated, LV_EVENT_READY, main_screen);
        spdlog::info("running touch calibration");
      } else {
        // load calibration data
        auto c = calibration_coeff.template get<std::vector<float>>();
        lv_tc_coeff_t coeff = {true, c[0], c[1], c[2], c[3], c[4], c[5]};
        lv_tc_set_coeff(coeff, false);
        spdlog::info("loaded calibration coefficients");
      }
    }
  }
#endif // OS_ANDROID

  return gs;
}

void PowerScreen::loop() {
  /*Handle LitlevGL tasks (tickless mode)*/
#if !defined(SIMULATOR) && !defined(OS_ANDROID)
  std::atomic_bool is_sleeping(false);
  Config *conf = Config::get_instance();
  int32_t display_sleep = conf->get<int32_t>("/display_sleep_sec") * 1000;
#endif

  while (1) {
    lv_lock.lock();
    lv_timer_handler();
    lv_lock.unlock();

#if !defined(SIMULATOR) && !defined(OS_ANDROID)
    if (display_sleep != -1) {
      if (lv_disp_get_inactive_time(NULL) > display_sleep) {
        if (!is_sleeping.load()) {
          spdlog::debug("putting display to sleeping");
          fbdev_blank();
          lv_obj_move_foreground(screen_saver);
          // spdlog::debug("screen saver foreground");
          is_sleeping = true;
        }
      } else {
        if (is_sleeping.load()) {
          spdlog::debug("waking up display");
          fbdev_unblank();
          lv_obj_move_background(screen_saver);
          is_sleeping = false;
        }
      }
    }
#endif  // SIMULATOR/OS_ANDROID

    usleep(5000);
  }
}

std::mutex &PowerScreen::get_lock() {
    return lv_lock;
}

void PowerScreen::connect_ws(const std::string &url) {
  init_panel.set_message("Waiting for printer to initialize...");
  ws.connect(url.c_str(),
   [this]() { init_panel.connected(ws); },
   [this]() { init_panel.disconnected(ws); });
}

void PowerScreen::new_theme_apply_cb(lv_theme_t *th, lv_obj_t *obj) {
  LV_UNUSED(th);

  if (lv_obj_check_type(obj, &lv_obj_class)) {
    lv_obj_add_style(obj, &style_container, 0);
  }

  if (lv_obj_check_type(obj, &lv_imgbtn_class)) {
//    lv_obj_add_style(obj, &style_imgbtn_default, LV_STATE_DEFAULT);
    lv_obj_add_style(obj, &style_imgbtn_pressed, LV_STATE_PRESSED);
    lv_obj_add_style(obj, &style_imgbtn_disabled, LV_STATE_DISABLED);
  }
}

void PowerScreen::handle_calibrated(lv_event_t *event) {
  spdlog::info("finished calibration");
  lv_obj_t *main_screen = (lv_obj_t *)event->user_data;
  lv_disp_load_scr(main_screen);
}

void PowerScreen::save_calibration_coeff(lv_tc_coeff_t coeff) {
  Config *conf = Config::get_instance();
  conf->set<std::vector<float>>("/touch_calibration_coeff",
                                {coeff.a, coeff.b, coeff.c, coeff.d, coeff.e, coeff.f});
  conf->save();
}

/*Set in lv_conf.h as `LV_TICK_CUSTOM_SYS_TIME_EXPR`*/
uint32_t custom_tick_get(void) {
  static uint64_t start_ms = 0;
  if (start_ms == 0) {
    struct timeval tv_start;
    gettimeofday(&tv_start, NULL);
    start_ms = (tv_start.tv_sec * 1000000 + tv_start.tv_usec) / 1000;
  }

  struct timeval tv_now;
  gettimeofday(&tv_now, NULL);
  uint64_t now_ms;
  now_ms = (tv_now.tv_sec * 1000000 + tv_now.tv_usec) / 1000;

  uint32_t time_ms = now_ms - start_ms;
  return time_ms;
}
