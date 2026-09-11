#ifndef __SQUARE_BUTTON_H__
#define __SQUARE_BUTTON_H__

#include "lvgl/lvgl.h"

class SquareButton {
 public:
  SquareButton(lv_obj_t *parent,
               const void *button_img,
               const char *text,
               lv_event_cb_t cb,
               void *user_data);
  ~SquareButton();

  lv_obj_t *get_button();
  void set_icon_color(lv_color_t color);
  void set_active(bool active, lv_color_t active_color);
  void disable();
  void enable();

 private:
  lv_obj_t *button;
  lv_obj_t *icon;
  lv_obj_t *label;
  lv_color_t icon_color;
  lv_color_t text_color;
};

#endif // __SQUARE_BUTTON_H__
