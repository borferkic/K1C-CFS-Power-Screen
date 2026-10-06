#ifndef __STATUS_ICONS_H__
#define __STATUS_ICONS_H__

#include "lvgl/lvgl.h"

#include <functional>

// Camera, filament and wifi indicators for the title bar.
// Active = accent green; inactive = muted gray with a diagonal slash.
class StatusIcons {
 public:
  // The icons are placed to the left of `anchor` (the clock label), right to left: wifi, filament, camera.
  StatusIcons(lv_obj_t *bar, lv_obj_t *anchor);

  void set_camera(bool active);
  void set_filament(bool active);
  void set_wifi(bool active);

  // The camera icon can be touched (the title bar is small, so the touch area is larger than the icon).
  void set_camera_handler(std::function<void()> handler);

 private:
  struct Item {
    lv_obj_t *wrapper = NULL;
    lv_obj_t *slash_cut = NULL;
    lv_obj_t *slash = NULL;
  };

  Item create_item(lv_obj_t *bar, const lv_img_dsc_t *src);
  void apply(Item &item, bool active);

  Item camera;
  Item filament;
  Item wifi;
  std::function<void()> camera_handler;
};

#endif // __STATUS_ICONS_H__
