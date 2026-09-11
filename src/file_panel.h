#ifndef __FILE_PANEL_H__
#define __FILE_PANEL_H__

#include "lvgl/lvgl.h"
#include "button_container.h"
#include "hv/json.hpp"

#include <string>

using json = nlohmann::json;

class FilePanel {
 public:
  FilePanel(lv_obj_t *parent);
  ~FilePanel();

  void foreground();
  void refresh_view(json &j, const std::string &gcode_path);  
  void set_last_printed(const std::string &last_printed);
  lv_obj_t *get_container();
  const char* get_thumbnail_path();

 private:
  lv_obj_t *file_cont;
  lv_obj_t *thumbnail;
  lv_obj_t *fname_label;
  lv_obj_t *detail_cont;
  lv_obj_t *print_time_value;
  lv_obj_t *filament_weight_value;
  lv_obj_t *last_printed_value;
};

#endif // __FILE_PANEL_H__
