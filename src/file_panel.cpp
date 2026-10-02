#include "file_panel.h"
#include <ctime>
#include "powerui.h"
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
  using namespace powerui;
  // PowerUI detail card: fixed preview frame, name, subtitle and three rows (print time, filament, layers).
  lv_obj_set_size(file_cont, LV_PCT(100), LV_PCT(100));
  lv_obj_clear_flag(file_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_layout(file_cont, 0);
  lv_obj_set_style_bg_opa(file_cont, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(file_cont, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(file_cont, 0, LV_PART_MAIN);

  lv_obj_t *preview = plain(file_cont);
  lv_obj_set_size(preview, LV_PCT(100), px(168));
  lv_obj_set_pos(preview, 0, 0);
  lv_obj_set_style_bg_color(preview, lv_color_hex(COLOR_BG), 0);
  lv_obj_set_style_bg_opa(preview, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(preview, px(10), 0);
  lv_obj_set_style_border_width(preview, 1, 0);
  lv_obj_set_style_border_color(preview, lv_color_hex(COLOR_WHITE), 0);
  lv_obj_set_style_border_opa(preview, LV_OPA_10, 0);
  lv_obj_set_parent(thumbnail, preview);
  lv_obj_center(thumbnail);
  lv_obj_set_style_border_width(thumbnail, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(thumbnail, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(thumbnail, LV_OPA_TRANSP, LV_PART_MAIN);

  lv_obj_set_width(fname_label, LV_PCT(100));
  lv_label_set_long_mode(fname_label, LV_LABEL_LONG_CLIP);
  lv_obj_set_pos(fname_label, 0, px(176));
  lv_obj_set_style_text_align(fname_label, LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_set_style_text_color(fname_label, lv_color_hex(COLOR_FG), 0);
  lv_obj_set_style_text_font(fname_label, &lv_font_montserrat_16, 0);

  fname_sub = lv_label_create(file_cont);
  lv_label_set_text(fname_sub, "");
  lv_obj_set_width(fname_sub, LV_PCT(100));
  lv_label_set_long_mode(fname_sub, LV_LABEL_LONG_CLIP);
  lv_obj_set_pos(fname_sub, 0, px(198));
  lv_obj_set_style_text_color(fname_sub, lv_color_hex(COLOR_MUTED), 0);
  lv_obj_set_style_text_font(fname_sub, &lv_font_montserrat_12, 0);

  layers_value = lv_label_create(detail_cont);
  lv_obj_set_layout(detail_cont, 0);
  lv_obj_clear_flag(detail_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(detail_cont, LV_PCT(100), px(102));
  lv_obj_set_pos(detail_cont, 0, px(222));
  lv_obj_set_style_bg_opa(detail_cont, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(detail_cont, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(detail_cont, 0, LV_PART_MAIN);

  const char *titles[] = {"Print time", "Filament", "Layers"};
  lv_obj_t *values[] = {print_time_value, filament_weight_value, layers_value};
  for (int i = 0; i < 3; ++i) {
    if (i > 0) {
      lv_obj_t *line = plain(detail_cont);
      lv_obj_set_size(line, LV_PCT(100), 1);
      lv_obj_set_pos(line, 0, px(i * 34));
      lv_obj_set_style_bg_color(line, lv_color_hex(COLOR_WHITE), 0);
      lv_obj_set_style_bg_opa(line, LV_OPA_10, 0);
    }
    lv_obj_t *title = label(detail_cont, titles[i], &lv_font_montserrat_14, lv_color_hex(COLOR_MUTED));
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, px(i * 34 + 8));

    lv_label_set_text(values[i], "(unknown)");
    lv_obj_set_width(values[i], px(150));
    lv_label_set_long_mode(values[i], LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_color(values[i], lv_color_hex(COLOR_FG), LV_PART_MAIN);
    lv_obj_set_style_text_font(values[i], &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_align(values[i], LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_align(values[i], LV_ALIGN_TOP_RIGHT, 0, px(i * 34 + 8));
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
  std::string shown_name = filename.string();
  const size_t extension = shown_name.rfind(".gcode");
  if (extension != std::string::npos && extension + 6 == shown_name.size()) {
    shown_name.erase(extension);
  }
  lv_label_set_text(fname_label, shown_name.c_str());
  
  const std::string print_time = eta > 0 ? KUtils::eta_string(eta) : "(unknown)";
  const std::string filament_weight = fweight > 0
    ? fmt::format("{:.1f} g", fweight)
    : "(unknown)";
  lv_label_set_text(print_time_value, print_time.c_str());
  lv_label_set_text(filament_weight_value, filament_weight.c_str());
  auto layers = j["/result/layer_count"_json_pointer];
  lv_label_set_text(layers_value, layers.is_number() ? fmt::format("{}", layers.template get<int>()).c_str() : "(unknown)");

  std::string sub;
  auto type = j["/result/filament_type"_json_pointer];
  if (type.is_string() && !type.template get<std::string>().empty()) {
    sub = type.template get<std::string>();
  }
  auto modified = j["/result/modified"_json_pointer];
  if (modified.is_number()) {
    const std::time_t stamp = static_cast<std::time_t>(modified.template get<double>());
    char date[16] = {};
    std::strftime(date, sizeof(date), "%b %d", std::localtime(&stamp));
    sub += std::string(sub.empty() ? "" : " - ") + "modified " + date;
  }
  lv_label_set_text(fname_sub, sub.c_str());

  auto width_scale = powerui::overlay_width_scale();
  auto thumb_detail = KUtils::get_thumbnail(gcode_path, j, width_scale);
  std::string fullpath = thumb_detail.first;    
  if (fullpath.length() > 0) {
    auto screen_width = powerui::overlay_width_px();
    size_t thumb_width = thumb_detail.second > 0 ? thumb_detail.second : 300;
    (void)screen_width;
    uint32_t normalized_thumb_scale = (150.0 / (double)thumb_width) * 256;
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
