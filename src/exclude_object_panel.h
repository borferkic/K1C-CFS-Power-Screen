#ifndef __EXCLUDE_OBJECT_PANEL_H__
#define __EXCLUDE_OBJECT_PANEL_H__

#include <mutex>

#include "lvgl/lvgl.h"
#include "notify_consumer.h"
#include "websocket_client.h"

#include <functional>
#include <mutex>
#include <string>
#include <vector>

// Exclude Object screen (PowerUI): the objects of the print on the left (with an Exclude button each) and a map of the
// bed on the right. It is fed by Klipper's `exclude_object` module (objects, excluded_objects, current_object) and
// excludes an object with EXCLUDE_OBJECT NAME=<name>. It opens from the Home print card.
class ExcludeObjectPanel : public NotifyConsumer {
 public:
  ExcludeObjectPanel(KWebSocketClient &ws, std::mutex &lock);
  ~ExcludeObjectPanel();

  void foreground();
  void consume(json &j);  // notify_status_update

  // Called (with the LVGL lock held) when the print gains or loses labeled objects.
  void set_availability_callback(std::function<void(bool)> callback);

  void handle_event(lv_event_t *event);
  static void _handle_event(lv_event_t *event) {
    static_cast<ExcludeObjectPanel *>(event->user_data)->handle_event(event);
  }

 private:
  struct Object {
    std::string name;
    double x0 = 0, y0 = 0, x1 = 0, y1 = 0;  // bounding box on the bed, mm
    bool excluded = false;
  };

  KWebSocketClient &ws;
  lv_obj_t *cont;
  lv_obj_t *list;
  lv_obj_t *map;
  lv_obj_t *empty;
  lv_obj_t *confirm;
  lv_obj_t *confirm_title;
  lv_obj_t *confirm_cancel;
  lv_obj_t *confirm_ok;
  lv_timer_t *sync_timer;
  std::string last_signature;
  bool last_available = false;
  std::vector<lv_obj_t *> exclude_buttons;

  std::vector<Object> objects;
  std::string current_object;
  double bed_min_x = 0, bed_min_y = 0, bed_max_x = 220, bed_max_y = 220;
  int pending = -1;  // object waiting for the confirmation
  std::function<void(bool)> availability_cb;

  void merge(const json &status);
  void sync_from_state();   // reads the full state (also loads a print that was already running)
  void update_availability();
  std::string signature() const;
  static void _sync_cb(lv_timer_t *timer) { static_cast<ExcludeObjectPanel *>(timer->user_data)->sync_from_state(); }
  void load_bed_size();
  void rebuild();
  void ask(int index);
  void do_exclude();
  static std::string display_name(const std::string &name);
};

#endif  // __EXCLUDE_OBJECT_PANEL_H__
