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
  void disable();
  void enable();

 private:
  lv_obj_t *button;
  lv_obj_t *icon;
  lv_obj_t *label;
};

#endif // __SQUARE_BUTTON_H__
