#ifndef __MINI_PRINT_STATUS__
#define __MINI_PRINT_STATUS__

#include "lvgl/lvgl.h"

#include <functional>
#include <string>

// PowerUI print card shown on the Home screen while a print is running (or on demand through the view toggle).
// It is fed by PrintStatusPanel; tapping the card opens the full Print Status screen.
class MiniPrintStatus {
 public:
  MiniPrintStatus(lv_obj_t *parent,
		  lv_event_cb_t cb,
		  void* user_data);

  ~MiniPrintStatus();

  // show(): a print is active (printing or paused). hide(): no active print.
  // The owner decides which Home view to display through the state callback.
  void show();
  void hide();
  lv_obj_t *get_container();
  lv_obj_t *get_toggle();

  void update_eta(uint32_t remaining_seconds);
  void update_eta_unknown();
  void update_status(std::string &status_str);
  void update_progress(int p);
  void update_name(const std::string &name);
  void update_material(const std::string &material);
  void update_elapsed(uint32_t elapsed_seconds);
  void update_layer(int current, int total);
  void update_speed(int percent);
  void reset();

  void set_state_callback(std::function<void(bool)> callback);
  void set_status_callback(std::function<void(const char *, lv_color_t)> callback);
  void set_actions(std::function<void()> pause, std::function<void()> resume, std::function<void()> stop);
  bool is_active() const;

  void handle_action(lv_event_t *event);
  static void _handle_action(lv_event_t *event) {
    MiniPrintStatus *card = (MiniPrintStatus *)event->user_data;
    card->handle_action(event);
  };

 private:
  void refresh_subtitle();

  lv_obj_t *cont;
  lv_obj_t *title_label;
  lv_obj_t *subtitle_label;
  lv_obj_t *toggle;
  lv_obj_t *state_badge;
  lv_obj_t *number_label;
  lv_obj_t *percent_label;
  lv_obj_t *eta_label;
  lv_obj_t *progress_bar;
  lv_obj_t *elapsed_value;
  lv_obj_t *layer_value;
  lv_obj_t *speed_value;
  lv_obj_t *pause_btn;
  lv_obj_t *pause_icon;
  lv_obj_t *pause_label;
  lv_obj_t *stop_btn;

  std::string status;
  std::string material;
  bool active;
  std::function<void(bool)> state_callback;
  std::function<void(const char *, lv_color_t)> status_callback;
  std::function<void()> pause_action;
  std::function<void()> resume_action;
  std::function<void()> stop_action;
};

#endif //__MINI_PRINT_STATUS__
