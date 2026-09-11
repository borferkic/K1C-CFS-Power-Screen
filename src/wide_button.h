#ifndef __WIDE_BUTTON_H__
#define __WIDE_BUTTON_H__

#include "lvgl/lvgl.h"

class WideButton {
 public:
  WideButton(lv_obj_t *parent,
             const void *button_img,
             const char *text,
             lv_event_cb_t cb,
             void *user_data);
  ~WideButton();

  lv_obj_t *get_button();

 private:
  lv_obj_t *button;
};

#endif // __WIDE_BUTTON_H__
