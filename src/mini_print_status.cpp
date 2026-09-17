#include "mini_print_status.h"
#include "spdlog/spdlog.h"

MiniPrintStatus::MiniPrintStatus(lv_obj_t *parent,
					 lv_event_cb_t cb,
					 void* user_data)
  : cont(lv_obj_create(parent))
  , progress_label_cont(lv_obj_create(cont))
  , progress_label(lv_label_create(progress_label_cont))
  , progress_label_bold(lv_label_create(progress_label_cont))
  , thumb(lv_img_create(cont))
  , status_label(lv_label_create(cont))
  , status("n/a")
  , eta("...")
{
  lv_obj_add_flag(cont, LV_OBJ_FLAG_HIDDEN);
  lv_color_t cur_bg = lv_obj_get_style_bg_color(cont, 0);
  lv_color_t mixed = lv_color_mix(lv_palette_main(LV_PALETTE_GREY),
				  cur_bg, LV_OPA_10);
  
  lv_obj_set_style_bg_color(cont, mixed, 0);  
  lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, 0);
  lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  auto scale = (double)lv_disp_get_physical_hor_res(NULL) / 800.0;

  
  lv_obj_set_size(cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_style_pad_top(cont, 2, 0);
  lv_obj_set_style_pad_bottom(cont, 2, 0);
  lv_obj_set_style_pad_left(cont, 4, 0);
  lv_obj_set_style_pad_right(cont, 4, 0);
  
  lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
  
  lv_obj_set_style_border_width(cont, 2, 0);
  lv_obj_set_style_border_color(cont, lv_color_hex(0x4CAF50), 0);
  lv_obj_set_style_border_opa(cont, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(cont, 4, 0);
  
  lv_obj_add_flag(cont, LV_OBJ_FLAG_FLOATING);
  lv_obj_align(cont, LV_ALIGN_TOP_LEFT, 0, -14 * scale);
  lv_obj_add_flag(cont, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(cont, cb, LV_EVENT_CLICKED, user_data);

  lv_label_set_text(status_label, fmt::format("ETA: {}\nStatus: {}", eta, status).c_str());

  auto progress_label_height = lv_font_get_line_height(&lv_font_montserrat_16);
  auto progress_label_pad_top = static_cast<lv_coord_t>(
    (40 * scale - progress_label_height) / 2);
  lv_obj_set_size(progress_label_cont, 40 * scale, 40 * scale);
  lv_obj_clear_flag(progress_label_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(progress_label_cont, 0, 0);
  lv_obj_set_style_bg_opa(progress_label_cont, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(progress_label_cont, 0, 0);

  for (auto label : {progress_label, progress_label_bold}) {
    lv_label_set_text(label, "0%");
    lv_obj_set_size(label, 40 * scale, 40 * scale);
    lv_obj_set_style_pad_top(label, progress_label_pad_top, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0x76FF03), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
  }
  lv_obj_align(progress_label, LV_ALIGN_CENTER, 0, 0);
  lv_obj_align(progress_label_bold, LV_ALIGN_CENTER, 1, 0);

  lv_img_set_size_mode(thumb, LV_IMG_SIZE_MODE_REAL);
  
}

MiniPrintStatus::~MiniPrintStatus() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}


void MiniPrintStatus::show() {
  lv_obj_clear_flag(cont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(cont);
}

void MiniPrintStatus::hide() {
  lv_obj_add_flag(cont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_background(cont);
}

lv_obj_t *MiniPrintStatus::get_container() {
  return cont;
}

void MiniPrintStatus::update_eta(std::string &eta_str) {
  eta = eta_str;
  lv_label_set_text(status_label, fmt::format("ETA: {}\nStatus: {}", eta, status).c_str());
}

void MiniPrintStatus::update_status(std::string &status_str) {
  status = status_str;
  lv_label_set_text(status_label, fmt::format("ETA: {}\nStatus: {}", eta, status).c_str());
}

void MiniPrintStatus::update_progress(int p) {
  auto progress_text = fmt::format("{}%", p);
  lv_label_set_text(progress_label, progress_text.c_str());
  lv_label_set_text(progress_label_bold, progress_text.c_str());
}

void MiniPrintStatus::update_img(const std::string &img_path, size_t twidth) {
  auto screen_width = lv_disp_get_physical_hor_res(NULL);
  uint32_t normalized_thumb_scale = ((0.05 * (double)screen_width) / (double)twidth) * 256;
  lv_img_set_zoom(thumb, normalized_thumb_scale);  
  lv_img_set_src(thumb, img_path.c_str());
}

void MiniPrintStatus::reset() {
  lv_label_set_text(progress_label, "0%");
  lv_label_set_text(progress_label_bold, "0%");

  // free src
  lv_img_set_src(thumb, NULL);
  // hack to color in empty space.
  ((lv_img_t*)thumb)->src_type = LV_IMG_SRC_SYMBOL;

  eta = "...";
  status = "n/a";  
}

