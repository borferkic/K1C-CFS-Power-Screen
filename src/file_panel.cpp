#include "file_panel.h"
#include "config.h"
#include "state.h"
#include "utils.h"
#include "spdlog/spdlog.h"

#include <experimental/filesystem>

namespace fs = std::experimental::filesystem;

#define THUMBSCALE = 0.78

FilePanel::FilePanel(lv_obj_t *parent)
  : file_cont(lv_obj_create(parent))
  , thumbnail(lv_img_create(file_cont))
  , fname_label(lv_label_create(file_cont))
  , detail_cont(lv_obj_create(file_cont))
  , print_time_value(lv_label_create(detail_cont))
  , filament_weight_value(lv_label_create(detail_cont))
{
  lv_obj_set_size(file_cont, LV_PCT(100), LV_PCT(100));
  lv_obj_clear_flag(file_cont, LV_OBJ_FLAG_SCROLLABLE);  
  lv_obj_align(file_cont, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_width(fname_label, LV_PCT(90));
  lv_label_set_long_mode(fname_label, LV_LABEL_LONG_SCROLL);
  lv_obj_set_style_text_align(fname_label, LV_TEXT_ALIGN_CENTER, 0);

  lv_obj_set_style_border_width(thumbnail, 2, LV_PART_MAIN);
  lv_obj_set_style_border_color(thumbnail, lv_color_hex(0x4CAF50), LV_PART_MAIN);
  lv_obj_set_style_border_opa(thumbnail, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(thumbnail, 6, LV_PART_MAIN);
  lv_obj_set_style_pad_all(thumbnail, 4, LV_PART_MAIN);

  static lv_coord_t grid_main_row_dsc[] = {
    LV_GRID_FR(7), LV_GRID_CONTENT, LV_GRID_FR(3), LV_GRID_TEMPLATE_LAST
  };
  static lv_coord_t grid_main_col_dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};

  lv_obj_set_grid_dsc_array(file_cont, grid_main_col_dsc, grid_main_row_dsc);

  lv_obj_set_grid_cell(thumbnail, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 0, 1);
  lv_obj_set_grid_cell(fname_label, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_START, 1, 1);
  lv_obj_set_width(detail_cont, LV_PCT(90));
  lv_obj_set_height(detail_cont, LV_SIZE_CONTENT);
  lv_obj_set_grid_cell(detail_cont, LV_GRID_ALIGN_STRETCH, 0, 1,
                       LV_GRID_ALIGN_CENTER, 2, 1);
  lv_obj_set_style_translate_y(detail_cont, -20, LV_PART_MAIN);

  lv_obj_clear_flag(detail_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(detail_cont, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_row(detail_cont, 4, LV_PART_MAIN);
  static lv_coord_t detail_row_dsc[] = {
    LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST
  };
  static lv_coord_t detail_col_dsc[] = {LV_GRID_CONTENT, LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
  lv_obj_set_grid_dsc_array(detail_cont, detail_col_dsc, detail_row_dsc);

  const char *titles[] = {"Print Time:", "Filament Weight:"};
  lv_obj_t *values[] = {print_time_value, filament_weight_value};
  for (uint32_t i = 0; i < 2; ++i) {
    lv_obj_t *title = lv_label_create(detail_cont);
    lv_label_set_text(title, titles[i]);
    lv_obj_set_style_text_color(title, lv_color_hex(0x4CAF50), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_set_grid_cell(title, LV_GRID_ALIGN_START, 0, 1,
                         LV_GRID_ALIGN_CENTER, i, 1);

    lv_label_set_text(values[i], "(unknown)");
    lv_obj_set_style_text_color(values[i], lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(values[i], &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_align(values[i], LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_set_width(values[i], LV_PCT(100));
    lv_obj_set_grid_cell(values[i], LV_GRID_ALIGN_END, 1, 1,
                         LV_GRID_ALIGN_CENTER, i, 1);
  }
}

FilePanel::~FilePanel() {
  if (file_cont != NULL) {
    lv_obj_del(file_cont);
    file_cont = NULL;
  }
}

void FilePanel::refresh_view(json &j, const std::string &gcode_path) {
  auto v = j["/result/estimated_time"_json_pointer];
  int eta = v.is_number() ? static_cast<int>(v.template get<double>()) : -1;
  v = j["/result/filament_weight_total"_json_pointer];
  double fweight = v.is_number() ? v.template get<double>() : -1.0;

  auto filename = fs::path(gcode_path).filename();
  lv_label_set_text(fname_label, filename.string().c_str());
  
  const std::string print_time = eta > 0 ? KUtils::eta_string(eta) : "(unknown)";
  const std::string filament_weight = fweight > 0
    ? fmt::format("{:.1f} g", fweight)
    : "(unknown)";
  lv_label_set_text(print_time_value, print_time.c_str());
  lv_label_set_text(filament_weight_value, filament_weight.c_str());

  auto width_scale = (double)lv_disp_get_physical_hor_res(NULL) / 800.0;
  auto thumb_detail = KUtils::get_thumbnail(gcode_path, j, width_scale);
  std::string fullpath = thumb_detail.first;    
  if (fullpath.length() > 0) {
    auto screen_width = lv_disp_get_physical_hor_res(NULL);
    size_t thumb_width = thumb_detail.second > 0 ? thumb_detail.second : 300;
    uint32_t normalized_thumb_scale = ((0.29 * (double)screen_width) / (double)thumb_width) * 256;
    thumbnail_source = "A:" + fullpath;
    lv_img_set_src(thumbnail, thumbnail_source.c_str());
    lv_img_set_zoom(thumbnail, normalized_thumb_scale);
  } else {
    thumbnail_source.clear();
    // free src
    lv_img_set_src(thumbnail, NULL);
    // hack to color in empty space.
    ((lv_img_t*)thumbnail)->src_type = LV_IMG_SRC_SYMBOL;
  }
}

void FilePanel::foreground() {
  lv_obj_move_foreground(file_cont);
}

lv_obj_t *FilePanel::get_container() {
  return file_cont;
}

const char* FilePanel::get_thumbnail_path() {
  return (const char*)lv_img_get_src(thumbnail);
}
