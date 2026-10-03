#include "print_panel.h"
#include "powerui.h"
#include "file_panel.h"
#include "state.h"
#include "utils.h"
#include "spdlog/spdlog.h"

#include <experimental/filesystem>
#include <fstream>
#include <map>
#include <sstream>

namespace fs = std::experimental::filesystem;

namespace {
const char *USB_DIR_NAME = "usb";  // same link name the installer creates (gcodes/usb)

// Mount point of the first mounted USB drive (/dev/sd*), or empty when there is none.
std::string usb_mount_point() {
  std::ifstream mounts("/proc/mounts");
  std::string line;
  while (std::getline(mounts, line)) {
    std::istringstream fields(line);
    std::string device, mount_point;
    fields >> device >> mount_point;
    if (device.rfind("/dev/sd", 0) == 0) {
      return mount_point;
    }
  }
  return "";
}
}

LV_IMG_DECLARE(print);
LV_IMG_DECLARE(ui_icon_play);
LV_IMG_DECLARE(back);

constexpr uint32_t CREALITY_GREEN = powerui::COLOR_ACCENT;

LV_IMG_DECLARE(sd_img);
LV_IMG_DECLARE(ui_icon_folder);

#define SORTED_BY_NAME 1 << 0
#define SORTED_BY_MODIFIED  1 << 1

PrintPanel::PrintPanel(KWebSocketClient &websocket, std::mutex &lock, PrintStatusPanel &ps, lv_obj_t *parent)
  : NotifyConsumer(lock)
  , ws(websocket)
  , files_cont(lv_obj_create(parent))
  , prompt_cont(lv_obj_create(lv_scr_act()))
  , msgbox(lv_obj_create(prompt_cont))
  , job_btn(lv_btn_create(msgbox))
  , cancel_btn(lv_btn_create(msgbox))
  , queue_btn(lv_btn_create(msgbox))
  , left_cont(lv_obj_create(files_cont))
  , file_table_btns(lv_obj_create(left_cont))
  , refresh_btn(lv_btn_create(file_table_btns))
  , modified_sort_btn(lv_btn_create(file_table_btns))
  , az_sort_btn(lv_btn_create(file_table_btns))
  , file_grid(lv_obj_create(left_cont))
  , file_view(lv_obj_create(files_cont))
  , print_btn(file_view, NULL, "Print", &PrintPanel::_handle_print_callback, this)
  , back_btn(file_view, &back, "Back", &PrintPanel::_handle_back_btn, this)
  , delete_context_cont(lv_obj_create(files_cont))
  , delete_context_menu(lv_obj_create(delete_context_cont))
  , delete_confirm_cont(lv_obj_create(files_cont))
  , delete_confirm_box(lv_obj_create(delete_confirm_cont))
  , delete_confirm_label(lv_label_create(delete_confirm_box))
  , delete_accept_btn(lv_btn_create(delete_confirm_box))
  , delete_cancel_btn(lv_btn_create(delete_confirm_box))
  , root("", "", 0)
  , cur_dir(&root)
  , cur_file(NULL)
  , delete_target(NULL)
  , file_panel(file_view)
  , print_status(ps)
  , sorted_by(SORTED_BY_MODIFIED)
{
  spdlog::trace("building print panel");
  // The file picker is the Files tab: it fills the tab page, so the Back button is not needed.
  lv_obj_set_size(files_cont, LV_PCT(100), LV_PCT(100));
  lv_obj_clear_flag(files_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(files_cont, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_all(files_cont, powerui::px(12), 0);
  lv_obj_set_style_pad_column(files_cont, powerui::px(12), 0);
  lv_obj_set_style_radius(files_cont, 0, 0);
  lv_obj_set_style_border_width(files_cont, 0, 0);
  lv_obj_set_style_bg_color(files_cont, lv_color_hex(powerui::COLOR_BG), 0);
  lv_obj_set_style_bg_opa(files_cont, LV_OPA_COVER, 0);

  // left side cont
  lv_obj_set_size(left_cont, powerui::px(408), LV_PCT(100));
  lv_obj_clear_flag(left_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(left_cont, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(left_cont, powerui::px(12), 0);
  lv_obj_set_style_bg_color(left_cont, lv_color_hex(powerui::COLOR_CARD), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(left_cont, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(left_cont, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(left_cont, lv_color_hex(powerui::COLOR_WHITE), LV_PART_MAIN);
  lv_obj_set_style_border_opa(left_cont, LV_OPA_10, LV_PART_MAIN);
  lv_obj_set_style_radius(left_cont, powerui::px(14), LV_PART_MAIN);
  lv_obj_set_style_pad_row(left_cont, powerui::px(8), LV_PART_MAIN);

  // file view buttons
  lv_obj_t * label = NULL;
  
  label = lv_label_create(refresh_btn);
  lv_label_set_text(label, LV_SYMBOL_REFRESH);
  lv_obj_center(label);

  label = lv_label_create(modified_sort_btn);
  lv_label_set_text(label, "Modified");
  lv_obj_center(label);

  label = lv_label_create(az_sort_btn);
  lv_label_set_text(label, "A-Z");
  lv_obj_center(label);

  lv_obj_add_event_cb(refresh_btn, &PrintPanel::_handle_btns, LV_EVENT_CLICKED, this);
  lv_obj_add_event_cb(modified_sort_btn, &PrintPanel::_handle_btns, LV_EVENT_CLICKED, this);
  lv_obj_add_event_cb(az_sort_btn, &PrintPanel::_handle_btns, LV_EVENT_CLICKED, this);
  
  lv_obj_set_size(file_table_btns, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_style_pad_all(file_table_btns, 4, 0);
  lv_obj_set_style_bg_color(file_table_btns, lv_color_hex(powerui::COLOR_BG), 0);
  lv_obj_set_style_bg_opa(file_table_btns, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(file_table_btns, 0, 0);
  lv_obj_set_style_radius(file_table_btns, 0, 0);

  lv_obj_clear_flag(file_table_btns, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(file_table_btns, LV_FLEX_FLOW_ROW);
  // Keep the 4 px margins and gaps in the 400 px toolbar.
  lv_obj_set_flex_align(file_table_btns, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_all(file_table_btns, 0, 0);
  lv_obj_set_style_pad_column(file_table_btns, powerui::px(8), 0);
  lv_obj_set_style_bg_opa(file_table_btns, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(file_table_btns, 0, 0);
  lv_obj_set_height(file_table_btns, powerui::px(40));

  lv_obj_t *sort_buttons[] = {refresh_btn, modified_sort_btn, az_sort_btn};
  for (lv_obj_t *sort_button : sort_buttons) {
    lv_obj_set_size(sort_button, sort_button == refresh_btn ? powerui::px(36) : (sort_button == modified_sort_btn ? powerui::px(88) : powerui::px(70)), powerui::px(36));
    lv_obj_set_style_pad_all(sort_button, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(sort_button, lv_color_hex(powerui::COLOR_FG), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(sort_button, lv_color_hex(powerui::COLOR_SECONDARY), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(sort_button, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(sort_button, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(sort_button, lv_color_hex(powerui::COLOR_PRESSED), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(sort_button, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(sort_button, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(sort_button, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
  }

  // Toolbar like the reference: [Modified][A-Z]   count   [refresh].
  count_label = lv_label_create(file_table_btns);
  lv_label_set_text(count_label, "");
  lv_label_set_long_mode(count_label, LV_LABEL_LONG_CLIP);
  lv_obj_set_flex_grow(count_label, 1);
  lv_obj_set_style_text_align(count_label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
  lv_obj_set_style_text_color(count_label, lv_color_hex(powerui::COLOR_MUTED), LV_PART_MAIN);
  lv_obj_set_style_text_font(count_label, &lv_font_montserrat_12, LV_PART_MAIN);
  lv_obj_move_to_index(modified_sort_btn, 0);
  lv_obj_move_to_index(az_sort_btn, 1);
  lv_obj_move_to_index(count_label, 2);
  lv_obj_move_to_index(refresh_btn, 3);
  update_sort_buttons();

  // Storage switch: Local / USB.
  storage_row = lv_obj_create(left_cont);
  lv_obj_remove_style_all(storage_row);
  lv_obj_clear_flag(storage_row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(storage_row, LV_PCT(100), powerui::px(40));
  lv_obj_set_style_pad_all(storage_row, 0, 0);
  lv_obj_set_style_pad_column(storage_row, 6, 0);
  lv_obj_set_flex_flow(storage_row, LV_FLEX_FLOW_ROW);
  auto make_storage_button = [this](const char *text) {
    lv_obj_t *button = lv_btn_create(storage_row);
    lv_obj_set_height(button, LV_PCT(100));
    lv_obj_set_flex_grow(button, 1);
    lv_obj_set_style_radius(button, powerui::px(8), LV_PART_MAIN);
    lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(button, 1, LV_PART_MAIN);
    lv_obj_t *button_label = lv_label_create(button);
    lv_obj_set_style_text_font(button_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_label_set_text(button_label, text);
    lv_obj_center(button_label);
    lv_obj_add_event_cb(button, &PrintPanel::_handle_btns, LV_EVENT_CLICKED, this);
    return button;
  };
  local_btn = make_storage_button("Local");
  usb_btn = make_storage_button("USB");
  timelapse_btn = make_storage_button("Timelapse");
  history_btn = make_storage_button("History");
  update_storage_buttons();
  lv_obj_move_to_index(storage_row, 0);

  lv_obj_set_width(file_grid, LV_PCT(100));
  lv_obj_set_height(file_grid, LV_SIZE_CONTENT);
  lv_obj_set_flex_grow(file_grid, 1);
  lv_obj_set_flex_flow(file_grid, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(file_grid, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_all(file_grid, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_row(file_grid, powerui::px(4), LV_PART_MAIN);
  lv_obj_set_style_pad_column(file_grid, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(file_grid, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(file_grid, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(file_grid, 0, LV_PART_MAIN);
  lv_obj_set_scroll_dir(file_grid, LV_DIR_VER);

  lv_obj_set_size(file_view, powerui::px(292), LV_PCT(100));
  lv_obj_clear_flag(file_view, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(file_view, lv_color_hex(powerui::COLOR_CARD), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(file_view, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(file_view, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(file_view, lv_color_hex(powerui::COLOR_WHITE), LV_PART_MAIN);
  lv_obj_set_style_border_opa(file_view, LV_OPA_10, LV_PART_MAIN);
  lv_obj_set_style_radius(file_view, powerui::px(14), LV_PART_MAIN);
  lv_obj_set_style_pad_all(file_view, powerui::px(12), LV_PART_MAIN);

  static lv_coord_t grid_main_row_dsc[] = {LV_GRID_FR(1), 60, LV_GRID_TEMPLATE_LAST};
  static lv_coord_t grid_main_col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
  lv_obj_set_grid_dsc_array(file_view, grid_main_col_dsc, grid_main_row_dsc);
  lv_obj_set_grid_cell(file_panel.get_container(), LV_GRID_ALIGN_STRETCH, 0, 3, LV_GRID_ALIGN_STRETCH, 0, 1);
  lv_obj_set_style_bg_opa(file_panel.get_container(), LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(file_panel.get_container(), 0, LV_PART_MAIN);

  lv_obj_set_grid_cell(print_btn.get_container(), LV_GRID_ALIGN_CENTER, 0, 3, LV_GRID_ALIGN_END, 1, 1);
  lv_obj_set_grid_cell(back_btn.get_container(), LV_GRID_ALIGN_CENTER, 2, 1, LV_GRID_ALIGN_END, 1, 1);
  lv_obj_add_flag(back_btn.get_container(), LV_OBJ_FLAG_HIDDEN);

  lv_obj_move_foreground(back_btn.get_container());
  lv_obj_move_foreground(print_btn.get_container());

  const lv_color_t file_button_grey = lv_color_hex(powerui::COLOR_SECONDARY);
  const lv_color_t file_button_green = lv_color_hex(powerui::COLOR_PRIMARY);
  const lv_color_t file_button_green_pressed = lv_color_hex(powerui::COLOR_PRIMARY_PRESSED);
  lv_obj_t *file_action_buttons[] = {
    print_btn.get_container(), back_btn.get_container()
  };
  const auto file_action_width = static_cast<lv_coord_t>(
    119 * powerui::overlay_width_scale());
  for (lv_obj_t *button : file_action_buttons) {
    lv_obj_set_width(button, file_action_width);
    lv_obj_set_style_bg_color(button, file_button_grey, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(button, lv_color_hex(powerui::COLOR_PRESSED), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(button, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(button, 12, LV_PART_MAIN);
    lv_obj_set_style_clip_corner(button, true, LV_PART_MAIN);
  }
  action_row = lv_obj_create(file_view);
  lv_obj_remove_style_all(action_row);
  lv_obj_clear_flag(action_row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(action_row, LV_PCT(100), powerui::px(56));
  lv_obj_set_flex_flow(action_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(action_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(action_row, powerui::px(12), 0);
  lv_obj_set_grid_cell(action_row, LV_GRID_ALIGN_STRETCH, 0, 3, LV_GRID_ALIGN_END, 1, 1);

  // Print: the word only, filled green (no icon).
  lv_obj_set_parent(print_btn.get_container(), action_row);
  print_btn.set_fixed_size(powerui::px(196), powerui::px(56));
  lv_obj_set_style_bg_opa(print_btn.get_button(), LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(print_btn.get_button(), LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_border_width(print_btn.get_button(), 0, LV_PART_MAIN);
  lv_obj_t *print_label = lv_obj_get_child(print_btn.get_container(), 1);
  if (print_label != NULL) {
    lv_obj_set_style_text_font(print_label, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(print_label, lv_color_hex(powerui::COLOR_WHITE), LV_PART_MAIN);
  }

  delete_btn = lv_btn_create(action_row);
  lv_obj_set_size(delete_btn, powerui::px(56), powerui::px(56));
  lv_obj_set_style_bg_color(delete_btn, lv_color_hex(powerui::COLOR_CARD), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(delete_btn, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(delete_btn, lv_color_hex(powerui::COLOR_SECONDARY), LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_border_width(delete_btn, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(delete_btn, lv_color_hex(powerui::COLOR_DESTRUCTIVE), LV_PART_MAIN);
  lv_obj_set_style_border_opa(delete_btn, LV_OPA_40, LV_PART_MAIN);
  lv_obj_set_style_radius(delete_btn, powerui::px(10), LV_PART_MAIN);
  lv_obj_set_style_shadow_width(delete_btn, 0, LV_PART_MAIN);
  lv_obj_t *delete_label = lv_label_create(delete_btn);
  lv_label_set_text(delete_label, LV_SYMBOL_TRASH);
  lv_obj_set_style_text_font(delete_label, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_set_style_text_color(delete_label, lv_color_hex(powerui::COLOR_DESTRUCTIVE), LV_PART_MAIN);
  lv_obj_center(delete_label);
  lv_obj_add_event_cb(delete_btn, &PrintPanel::_handle_btns, LV_EVENT_CLICKED, this);
  lv_obj_set_style_bg_color(print_btn.get_container(), file_button_green,
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(print_btn.get_container(), file_button_green_pressed,
                            LV_PART_MAIN | LV_STATE_PRESSED);

  // Context menu shown beside a file after a long press.
  lv_obj_add_flag(delete_context_cont, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_set_size(delete_context_cont, LV_PCT(100), LV_PCT(100));
  lv_obj_set_pos(delete_context_cont, 0, 0);
  lv_obj_set_style_pad_all(delete_context_cont, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(delete_context_cont, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(delete_context_cont, 0, LV_PART_MAIN);
  lv_obj_add_flag(delete_context_cont, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(delete_context_cont, &PrintPanel::_handle_btns,
                      LV_EVENT_CLICKED, this);
  lv_obj_set_size(delete_context_menu, 128, 52);
  lv_obj_set_style_bg_color(delete_context_menu, lv_color_hex(powerui::COLOR_CARD), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(delete_context_menu, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(delete_context_menu, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(delete_context_menu, lv_color_hex(powerui::COLOR_WHITE), LV_PART_MAIN);
  lv_obj_set_style_border_opa(delete_context_menu, LV_OPA_30, LV_PART_MAIN);
  lv_obj_set_style_radius(delete_context_menu, 12, LV_PART_MAIN);
  lv_obj_set_style_clip_corner(delete_context_menu, true, LV_PART_MAIN);
  lv_obj_add_flag(delete_context_menu, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(delete_context_menu, &PrintPanel::_handle_btns,
                      LV_EVENT_CLICKED, this);
  label = lv_label_create(delete_context_menu);
  lv_label_set_text(label, "Delete");
  lv_obj_set_style_text_color(label, lv_color_hex(powerui::COLOR_DESTRUCTIVE), LV_PART_MAIN);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_center(label);
  lv_obj_add_flag(delete_context_cont, LV_OBJ_FLAG_HIDDEN);

  // Confirmation dialog shown after selecting Delete.
  lv_obj_add_flag(delete_confirm_cont, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_set_size(delete_confirm_cont, LV_PCT(100), LV_PCT(100));
  lv_obj_set_pos(delete_confirm_cont, 0, 0);
  lv_obj_set_style_pad_all(delete_confirm_cont, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_color(delete_confirm_cont, lv_color_hex(powerui::COLOR_BLACK), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(delete_confirm_cont, LV_OPA_70, LV_PART_MAIN);
  lv_obj_set_style_border_width(delete_confirm_cont, 0, LV_PART_MAIN);
  lv_obj_add_flag(delete_confirm_cont, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(delete_confirm_cont, &PrintPanel::_handle_btns,
                      LV_EVENT_CLICKED, this);

  // PowerUI dialog: dark card, title, file name and a muted note; Cancel on the left, Delete (red) on the right.
  using powerui::px;
  lv_obj_set_size(delete_confirm_box, px(380), px(196));
  lv_obj_align(delete_confirm_box, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_pad_all(delete_confirm_box, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_color(delete_confirm_box, lv_color_hex(powerui::COLOR_CARD), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(delete_confirm_box, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(delete_confirm_box, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(delete_confirm_box, lv_color_hex(powerui::COLOR_WHITE), LV_PART_MAIN);
  lv_obj_set_style_border_opa(delete_confirm_box, LV_OPA_30, LV_PART_MAIN);
  lv_obj_set_style_radius(delete_confirm_box, px(14), LV_PART_MAIN);
  lv_obj_clear_flag(delete_confirm_box, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *delete_title = powerui::label(delete_confirm_box, "Delete file?", &lv_font_montserrat_20, lv_color_hex(powerui::COLOR_FG));
  lv_obj_set_pos(delete_title, px(24), px(22));

  lv_obj_set_width(delete_confirm_label, px(332));
  lv_obj_set_height(delete_confirm_label, px(52));
  lv_label_set_long_mode(delete_confirm_label, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_align(delete_confirm_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
  lv_obj_set_style_text_color(delete_confirm_label, lv_color_hex(powerui::COLOR_MUTED), LV_PART_MAIN);
  lv_obj_set_style_text_font(delete_confirm_label, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_set_pos(delete_confirm_label, px(24), px(58));

  auto style_dialog_button = [](lv_obj_t *btn, uint32_t bg, uint32_t pressed, int x) {
    lv_obj_set_size(btn, px(156), px(44));
    lv_obj_set_pos(btn, px(x), px(132));
    lv_obj_set_style_bg_color(btn, lv_color_hex(bg), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btn, lv_color_hex(pressed), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(btn, px(10), LV_PART_MAIN);
  };
  style_dialog_button(delete_cancel_btn, powerui::COLOR_SECONDARY, powerui::COLOR_PRESSED, 24);
  lv_obj_t *cancel_label = powerui::label(delete_cancel_btn, "Cancel", &lv_font_montserrat_16, lv_color_hex(powerui::COLOR_FG));
  lv_obj_center(cancel_label);
  style_dialog_button(delete_accept_btn, powerui::COLOR_DANGER, powerui::COLOR_MAT_RED_DARK, 200);
  lv_obj_t *accept_label = powerui::label(delete_accept_btn, "Delete", &lv_font_montserrat_16, lv_color_hex(powerui::COLOR_WHITE));
  lv_obj_center(accept_label);

  lv_obj_add_event_cb(delete_accept_btn, &PrintPanel::_handle_btns,
                      LV_EVENT_CLICKED, this);
  lv_obj_add_event_cb(delete_cancel_btn, &PrintPanel::_handle_btns,
                      LV_EVENT_CLICKED, this);
  lv_obj_add_flag(delete_confirm_cont, LV_OBJ_FLAG_HIDDEN);

  // prompt
  lv_obj_add_flag(prompt_cont, LV_OBJ_FLAG_HIDDEN);  
  lv_obj_set_size(prompt_cont, LV_PCT(100), LV_PCT(100));
  lv_obj_clear_flag(prompt_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_opa(prompt_cont, LV_OPA_70, 0);

  lv_obj_set_size(msgbox, LV_PCT(60), LV_PCT(30));
  lv_obj_set_style_border_width(msgbox, 2, 0);
  lv_obj_set_style_bg_color(msgbox, lv_color_hex(powerui::COLOR_CARD), 0);
  lv_obj_set_style_border_color(msgbox, lv_color_hex(powerui::COLOR_WHITE), 0);
  lv_obj_set_style_border_opa(msgbox, LV_OPA_30, 0);
  lv_obj_set_style_border_width(msgbox, 1, 0);
  lv_obj_set_style_radius(msgbox, powerui::px(14), 0);
  lv_obj_set_style_pad_all(msgbox, powerui::px(14), 0);
  lv_obj_set_style_bg_color(prompt_cont, lv_color_hex(powerui::COLOR_BLACK), 0);
  
  lv_obj_align(msgbox, LV_ALIGN_CENTER, 0, 0);

  lv_obj_add_event_cb(job_btn, &PrintPanel::_handle_btns, LV_EVENT_CLICKED, this);
  lv_obj_align(job_btn, LV_ALIGN_BOTTOM_MID, 0, 0);

  lv_obj_add_event_cb(cancel_btn, &PrintPanel::_handle_btns, LV_EVENT_CLICKED, this);
  lv_obj_align(cancel_btn, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

  lv_obj_add_event_cb(queue_btn, &PrintPanel::_handle_btns, LV_EVENT_CLICKED, this);
  lv_obj_align(queue_btn, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  
  // PowerUI look: View Job is the main action (soft green), Queue Job and Cancel are outline.
  const int dlg_btn_w = LV_PCT(30);
  lv_obj_t *dlg_btns[] = {job_btn, cancel_btn, queue_btn};
  const char *dlg_texts[] = {"View Job", "Cancel", "Queue Job"};
  for (int i = 0; i < 3; i++) {
    lv_obj_set_size(dlg_btns[i], dlg_btn_w, powerui::px(44));
    label = lv_label_create(dlg_btns[i]);
    lv_label_set_text(label, dlg_texts[i]);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
    lv_obj_center(label);
    powerui::style_button(dlg_btns[i], label, i == 0 ? powerui::ButtonKind::Soft : powerui::ButtonKind::Outline);
  }

  label = lv_label_create(msgbox);
  lv_label_set_text(label, "Printing in progress...");
  lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(label, lv_color_hex(powerui::COLOR_FG), 0);
  lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 0);

  ws.register_notify_update(this);

  // Timelapse and History views (hidden until their segment is selected).
  extra_view.reset(new FilesExtraView(ws, lv_lock, left_cont, files_cont, [this](const std::string &file) {
    begin_print_flow(file);
  }));
}

PrintPanel::~PrintPanel() {
  if (files_cont != NULL) {
    lv_obj_del(files_cont);
    files_cont = NULL;
  }

  if (prompt_cont != NULL) {
    lv_obj_del(prompt_cont);
    prompt_cont = NULL;
  }
}

void PrintPanel::populate_files(json &j) {
  sorted_by = SORTED_BY_MODIFIED;
  show_dir(cur_dir, SORTED_BY_MODIFIED);
}

void PrintPanel::consume(json &j) {
  (void)j;
}

void PrintPanel::subscribe() {
  sync_usb_link();
  ws.send_jsonrpc("server.files.list", R"({"root":"gcodes"})"_json, [this](json &d) {
    std::lock_guard<std::mutex> lock(lv_lock);
    std::string cur_path = cur_dir->full_path;
    root.clear();
    cur_file = NULL;
    cur_dir = NULL;

    if (d.contains("result")) {
      for (auto f : d["result"]) {
        root.add_path(KUtils::split(f["path"], '/'), f["path"], f["modified"].template get<uint32_t>());
      }
    }
    Tree *dir = root.find_path(KUtils::split(cur_path, '/'));
    // need to simply this using the directory endpoint
    cur_dir = dir;

    // Local shows everything except the USB folder; USB shows only that folder.
    Tree *usb_node = root.get_child(USB_DIR_NAME);
    usb_missing = usb_view && usb_node == NULL;
    if (usb_view) {
      if (usb_node != NULL && (cur_dir == NULL || cur_dir == &root)) {
        cur_dir = usb_node;
      }
    } else if (cur_dir == NULL || (cur_path.rfind(USB_DIR_NAME, 0) == 0)) {
      cur_dir = &root;
    }
    if (cur_dir == NULL) {
      cur_dir = &root;
    }
    this->populate_files(d);
  });
}

void PrintPanel::foreground() {
  json &pstat_state = State::get_instance()
    ->get_data("/printer_state/print_stats/state"_json_pointer);
  spdlog::debug("print panel print stats {}",
		pstat_state.is_null() ? "nil" : pstat_state.template get<std::string>());
    
  lv_obj_move_foreground(files_cont);
  if (view_mode >= 2 && extra_view) {
    show_extra(view_mode == 2 ? FilesExtraView::Mode::Timelapse : FilesExtraView::Mode::History);
  }
}

void PrintPanel::background() {
  lv_obj_move_background(files_cont);
}

void PrintPanel::handle_callback(lv_event_t *e) {
  (void)e;
}

void PrintPanel::show_dir(Tree *dir, uint32_t sort_type) {
  hide_delete_context();
  hide_delete_confirmation();
  delete_target = NULL;
  file_cards.clear();
  lv_obj_clean(file_grid);
  sort_modified = (sort_type == SORTED_BY_MODIFIED);
  update_sort_buttons();
  lv_label_set_text(count_label, usb_missing ? "" : fmt::format("{} - {} files", usb_view ? "USB" : "Local", dir->children.size()).c_str());

  if (usb_missing) {
    cur_file = NULL;
    powerui::empty_state(file_grid, &sd_img, "No USB drive detected", "Insert a USB drive and tap the reload button.");
    return;
  }
  if (dir->children.empty()) {
    cur_file = NULL;
    powerui::empty_state(file_grid, &ui_icon_folder, "No files here", "Upload G-code from Fluidd or a USB drive.");
    return;
  }
  file_cards.reserve(dir->children.size());

  auto create_card = [this](Tree *node, const std::string &path, bool directory) {
    lv_obj_t *card = lv_obj_create(file_grid);
    lv_obj_set_width(card, LV_PCT(100));
    lv_obj_set_height(card, powerui::px(56));
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(card, &PrintPanel::_handle_file_card, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(card, &PrintPanel::_handle_file_card, LV_EVENT_LONG_PRESSED, this);
    lv_obj_set_style_bg_opa(card, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(card, lv_color_hex(powerui::COLOR_SECONDARY), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(card, lv_color_hex(powerui::COLOR_SECONDARY), LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_set_style_border_width(card, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(card, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(card, lv_color_hex(CREALITY_GREEN), LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_set_style_border_opa(card, LV_OPA_50, LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_set_style_radius(card, powerui::px(10), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(card, powerui::px(8), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(card, powerui::px(12), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *thumbnail_frame = lv_obj_create(card);
    lv_obj_set_size(thumbnail_frame, powerui::px(40), powerui::px(40));
    lv_obj_clear_flag(thumbnail_frame, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(thumbnail_frame, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(thumbnail_frame, lv_color_hex(powerui::COLOR_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(thumbnail_frame, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(thumbnail_frame, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(thumbnail_frame, lv_color_hex(powerui::COLOR_WHITE), LV_PART_MAIN);
    lv_obj_set_style_border_opa(thumbnail_frame, LV_OPA_10, LV_PART_MAIN);
    lv_obj_set_style_radius(thumbnail_frame, powerui::px(8), LV_PART_MAIN);
    lv_obj_set_style_pad_all(thumbnail_frame, 0, LV_PART_MAIN);

    lv_obj_t *thumbnail = lv_img_create(thumbnail_frame);
    lv_img_set_size_mode(thumbnail, LV_IMG_SIZE_MODE_REAL);
    lv_obj_clear_flag(thumbnail, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(thumbnail, directory ? LV_SYMBOL_DIRECTORY : LV_SYMBOL_IMAGE);
    lv_obj_set_size(thumbnail, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_text_font(thumbnail, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_img_recolor(thumbnail,
                                 directory ? lv_color_hex(CREALITY_GREEN) : lv_color_hex(powerui::COLOR_WHITE),
                                 LV_PART_MAIN);
    lv_obj_set_style_img_recolor_opa(thumbnail, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_center(thumbnail);

    lv_obj_t *text_col = lv_obj_create(card);
    lv_obj_remove_style_all(text_col);
    lv_obj_clear_flag(text_col, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(text_col, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_height(text_col, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(text_col, 1);
    lv_obj_set_flex_flow(text_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(text_col, 2, 0);

    std::string display_name = node == NULL ? ".." : node->name;
    const size_t extension = display_name.rfind(".gcode");
    if (extension != std::string::npos && extension + 6 == display_name.size()) {
      display_name.erase(extension);
    }
    lv_obj_t *name_label = lv_label_create(text_col);
    lv_obj_clear_flag(name_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_width(name_label, LV_PCT(100));
    lv_label_set_long_mode(name_label, LV_LABEL_LONG_CLIP);
    lv_label_set_text(name_label, display_name.c_str());
    lv_obj_set_style_text_color(name_label, lv_color_hex(powerui::COLOR_FG), LV_PART_MAIN);
    lv_obj_set_style_text_font(name_label, &lv_font_montserrat_14, LV_PART_MAIN);

    lv_obj_t *subtitle_label = lv_label_create(text_col);
    lv_obj_clear_flag(subtitle_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_width(subtitle_label, LV_PCT(100));
    lv_label_set_long_mode(subtitle_label, LV_LABEL_LONG_CLIP);
    lv_label_set_text(subtitle_label, directory ? "Folder" : "");
    lv_obj_set_style_text_color(subtitle_label, lv_color_hex(powerui::COLOR_MUTED), LV_PART_MAIN);
    lv_obj_set_style_text_font(subtitle_label, &lv_font_montserrat_12, LV_PART_MAIN);

    lv_obj_t *chevron = lv_label_create(card);
    lv_obj_clear_flag(chevron, LV_OBJ_FLAG_CLICKABLE);
    lv_label_set_text(chevron, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_color(chevron, lv_color_hex(powerui::COLOR_MUTED), LV_PART_MAIN);
    lv_obj_set_style_text_font(chevron, &lv_font_montserrat_14, LV_PART_MAIN);

    file_cards.push_back({card, thumbnail, path, node, directory, "", subtitle_label});
    if (!directory && node->contains_metadata()) {
      update_file_card(path, node->metadata);
    }
  };

  bool reversed = sorted_by & sort_type;
  std::vector<Tree> sorted_files;
  if (sort_type == SORTED_BY_MODIFIED) {
    KUtils::sort_map_values<std::string, Tree>(dir->children, sorted_files, [reversed](Tree &x, Tree &y) {
	if (x.is_leaf() && !y.is_leaf()) {
	  return false;
	} else if (!x.is_leaf() && y.is_leaf()) {
	  return true;
	}

	return reversed ? x.date_modified > y.date_modified : y.date_modified > x.date_modified;
      });
  } else {
    KUtils::sort_map_values<std::string, Tree>(dir->children, sorted_files, [reversed](Tree &x, Tree &y) {
	if (x.is_leaf() && !y.is_leaf()) {
	  return false;
	} else if (!x.is_leaf() && y.is_leaf()) {
	  return true;
      }

      return reversed ? x.name > y.name : y.name > x.name;
      });
  }

  sorted_by = (sorted_by ^ sort_type) & sort_type;
  for (const auto &c : sorted_files) {
    if (!usb_view && dir == &root && c.name == USB_DIR_NAME) {
      continue;  // the USB link is only shown in the USB view
    }
    Tree *node = dir->get_child(c.name);
    if (node != NULL) {
      create_card(node, node->full_path, !node->is_leaf());
      if (node->is_leaf() && !node->contains_metadata()) {
        request_file_metadata(node);
      }
    }
  }

  lv_obj_scroll_to_y(file_grid, 0, LV_ANIM_OFF);

  // XXX: maybe use the directory instead of file endpoint in moonraker
  cur_file = NULL;
  for (auto &c : sorted_files) {
    if (c.is_leaf()) {
      const auto &selected = dir->children.find(c.name);
      if (selected != dir->children.cend()) {
	cur_file = &selected->second;
	for (auto &card : file_cards) {
	  if (card.node == cur_file) {
	    select_file_card(card);
	    break;
	  }
	}
      }
      break;
    }
  }

}

void PrintPanel::handle_file_card(lv_event_t *event) {
  lv_event_code_t code = lv_event_get_code(event);
  lv_obj_t *target = lv_event_get_current_target(event);

  if (code == LV_EVENT_LONG_PRESSED) {
    for (auto &card : file_cards) {
      if (card.card == target && card.node != NULL && card.node->is_leaf()) {
        delete_target = card.node;
        show_delete_context(card);
        return;
      }
    }
    return;
  }

  if (code != LV_EVENT_CLICKED) {
    return;
  }

  for (auto &card : file_cards) {
    if (card.card != target) {
      continue;
    }

    if (card.node == NULL) {
      if (cur_dir->parent != cur_dir) {
        cur_dir = cur_dir->parent;
        show_dir(cur_dir, sorted_by);
      }
    } else if (!card.node->is_leaf()) {
      cur_dir = card.node;
      show_dir(cur_dir, sorted_by);
    } else {
      select_file_card(card);
    }
    return;
  }
}

void PrintPanel::show_delete_context(FileCard &card) {
  hide_delete_confirmation();

  lv_area_t card_area;
  lv_area_t parent_area;
  lv_obj_get_coords(card.card, &card_area);
  lv_obj_get_coords(files_cont, &parent_area);

  const lv_coord_t menu_width = lv_obj_get_width(delete_context_menu);
  const lv_coord_t menu_height = lv_obj_get_height(delete_context_menu);
  const lv_coord_t parent_width = lv_obj_get_width(files_cont);
  const lv_coord_t parent_height = lv_obj_get_height(files_cont);
  lv_coord_t menu_x = card_area.x2 - parent_area.x1 + 6;
  lv_coord_t menu_y = card_area.y1 - parent_area.y1;

  if (menu_x + menu_width > parent_width - 4) {
    menu_x = card_area.x1 - parent_area.x1 - menu_width - 6;
  }
  if (menu_x < 4) {
    menu_x = 4;
  }
  if (menu_y + menu_height > parent_height - 4) {
    menu_y = parent_height - menu_height - 4;
  }
  if (menu_y < 4) {
    menu_y = 4;
  }

  lv_obj_set_pos(delete_context_menu, menu_x, menu_y);
  lv_obj_clear_flag(delete_context_cont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(delete_context_cont);
}

void PrintPanel::show_delete_confirmation() {
  if (delete_target == NULL) {
    return;
  }

  std::string message = delete_target->name + "\nThis cannot be undone.";
  lv_label_set_text(delete_confirm_label, message.c_str());
  hide_delete_context();
  lv_obj_clear_state(delete_accept_btn, LV_STATE_DISABLED);
  lv_obj_clear_flag(delete_confirm_cont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(delete_confirm_cont);
}

void PrintPanel::hide_delete_context() {
  if (delete_context_cont != NULL) {
    lv_obj_add_flag(delete_context_cont, LV_OBJ_FLAG_HIDDEN);
  }
}

void PrintPanel::hide_delete_confirmation() {
  if (delete_confirm_cont != NULL) {
    lv_obj_add_flag(delete_confirm_cont, LV_OBJ_FLAG_HIDDEN);
  }
}

void PrintPanel::select_file_card(FileCard &card) {
  if (card.node == NULL) {
    return;
  }

  cur_file = card.node;
  for (auto &item : file_cards) {
    if (item.card == card.card) {
      lv_obj_add_state(item.card, LV_STATE_CHECKED);
    } else {
      lv_obj_clear_state(item.card, LV_STATE_CHECKED);
    }
  }
  show_file_detail(cur_file);
}

void PrintPanel::request_file_metadata(Tree *file) {
  const std::string path = file->full_path;
  ws.send_jsonrpc("server.files.metadata",
                  json{{"filename", path}},
                  [this, path](json &j) {
                    if (!j.contains("result")) {
                      return;
                    }
                    std::lock_guard<std::mutex> lock(lv_lock);
                    for (auto &card : file_cards) {
                      if (card.path == path && card.node != NULL) {
                        card.node->set_metadata(j);
                        update_file_card(path, j);
                        return;
                      }
                    }
                  });
}

void PrintPanel::update_file_card(const std::string &path, json &metadata) {
  for (auto &card : file_cards) {
    if (card.path != path) {
      continue;
    }

    {
      auto eta_json = metadata["/result/estimated_time"_json_pointer];
      auto weight_json = metadata["/result/filament_weight_total"_json_pointer];
      if (card.subtitle != NULL && !card.directory && eta_json.is_number()) {
        std::string text = KUtils::eta_string(static_cast<int>(eta_json.template get<double>()));
        const size_t seconds = text.rfind(' ');
        if (seconds != std::string::npos && text.back() == 's' && text.find('m') != std::string::npos) {
          text.erase(seconds);  // keep hours and minutes only
        }
        if (weight_json.is_number()) {
          text += fmt::format(" - {:.0f} g", weight_json.template get<double>());
        }
        lv_label_set_text(card.subtitle, text.c_str());
      }
    }
    auto thumb_detail = KUtils::get_thumbnail(path, metadata, 96.0 / 300.0);
    if (!thumb_detail.first.empty()) {
      card.thumbnail_source = "A:" + thumb_detail.first;
      lv_img_cache_invalidate_src(card.thumbnail_source.c_str());
      lv_img_set_src(card.thumbnail, card.thumbnail_source.c_str());
      lv_obj_set_style_img_recolor_opa(card.thumbnail, LV_OPA_TRANSP, LV_PART_MAIN);
      lv_obj_set_size(card.thumbnail, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
      lv_img_set_zoom(card.thumbnail, 100);  // fits the 40 px frame
      lv_obj_center(card.thumbnail);
      lv_obj_invalidate(card.thumbnail);
    }
    return;
  }
}

void PrintPanel::show_file_detail(Tree *f) {
  if (f->is_leaf()) {
    if (f->contains_metadata()) {
      file_panel.refresh_view(f->metadata, f->full_path);
    } else {
      spdlog::trace("getting metadata for {}", f->name);
      const std::string path = f->full_path;
      ws.send_jsonrpc("server.files.metadata",
                      json{{"filename", path}},
                      [this, path](json &d) { this->handle_metadata(path, d); });
    }
  }
}

void PrintPanel::handle_metadata(const std::string &path, json &j) {
  spdlog::trace("handling metadata callback");  
  if (!j.contains("result")) {
    return;
  }

  std::lock_guard<std::mutex> lock(lv_lock);
  for (auto &card : file_cards) {
    if (card.path == path && card.node != NULL) {
      card.node->set_metadata(j);
      update_file_card(path, j);
      if (cur_file == card.node) {
        file_panel.refresh_view(card.node->metadata, card.node->full_path);
      }
      return;
    }
  }
}

void PrintPanel::handle_back_btn(lv_event_t *event) {
  lv_obj_t *btn = lv_event_get_current_target(event);
  if (btn == back_btn.get_container()) {
    lv_obj_move_background(files_cont);
    print_status.background();    
  }
}

void PrintPanel::handle_print_callback(lv_event_t *event) {
  lv_event_code_t code = lv_event_get_code(event);
  if (code == LV_EVENT_CLICKED && cur_file != NULL) {

    json &pstat_state = State::get_instance()
      ->get_data("/printer_state/print_stats/state"_json_pointer);
    spdlog::debug("print panel print stats {}",
		  pstat_state.is_null() ? "nil" : pstat_state.template get<std::string>());
    
    if (!pstat_state.is_null()
	&& pstat_state.template get<std::string>() != "printing"
	&& pstat_state.template get<std::string>() != "paused") {
      spdlog::debug("printer ready to print. print file {}", cur_file->full_path);
	
      // ws.send_jsonrpc("printer.gcode.script",
      // 		    json::parse(R"({"script":"PRINT_PREPARE_CLEAR"})"));

      begin_print_flow(cur_file->full_path);

    } else {
      lv_obj_clear_flag(prompt_cont, LV_OBJ_FLAG_HIDDEN);
      lv_obj_move_foreground(prompt_cont);
    }
  }
}

void PrintPanel::handle_btns(lv_event_t *event) {
  lv_event_code_t code = lv_event_get_code(event);
  if (code == LV_EVENT_CLICKED) {
    lv_obj_t *btn = lv_event_get_current_target(event);

    if (btn == delete_btn) {
      if (cur_file != NULL) {
        delete_target = cur_file;
        show_delete_confirmation();
      }
      return;
    }

    if (btn == delete_context_cont) {
      hide_delete_context();
      delete_target = NULL;
      return;
    }

    if (btn == delete_context_menu) {
      show_delete_confirmation();
      return;
    }

    if (btn == delete_cancel_btn) {
      hide_delete_confirmation();
      delete_target = NULL;
      return;
    }

    if (btn == delete_confirm_cont) {
      return;
    }

    if (btn == delete_accept_btn) {
      if (delete_target == NULL) {
        hide_delete_confirmation();
        return;
      }

      const std::string path = "gcodes/" + delete_target->full_path;
      lv_obj_add_state(delete_accept_btn, LV_STATE_DISABLED);
      ws.send_jsonrpc("server.files.delete_file", json{{"path", path}},
                      [this](json &response) {
                        std::lock_guard<std::mutex> lock(lv_lock);
                        if (response.contains("error")) {
                          spdlog::error("failed to delete file: {}", response.dump());
                          lv_obj_clear_state(delete_accept_btn, LV_STATE_DISABLED);
                          return;
                        }

                        delete_target = NULL;
                        hide_delete_confirmation();
                        lv_obj_clear_state(delete_accept_btn, LV_STATE_DISABLED);
                        subscribe();
                      });
      return;
    }

    if (cur_file != NULL) {
      spdlog::trace("status prompt clicked");
      if (btn == queue_btn) {
	spdlog::trace("status prompt queue clicked");
      }

      if (btn == job_btn) {
	spdlog::trace("status prompt job clicked");
      }

      if (btn == cancel_btn) {
	spdlog::trace("status prompt cancel clicked");
	lv_obj_move_background(prompt_cont);
	lv_obj_add_flag(prompt_cont, LV_OBJ_FLAG_HIDDEN);
      }
    }

    if (btn == local_btn) {
      set_storage(false);
    } else if (btn == usb_btn) {
      set_storage(true);
    } else if (btn == timelapse_btn) {
      show_extra(FilesExtraView::Mode::Timelapse);
    } else if (btn == history_btn) {
      show_extra(FilesExtraView::Mode::History);
    } else if (btn == refresh_btn) {
      subscribe();
      
    } else if (btn == modified_sort_btn) {
      show_dir(cur_dir, SORTED_BY_MODIFIED);

    } else if (btn == az_sort_btn) {
      show_dir(cur_dir, SORTED_BY_NAME);
    }
  }
}


void PrintPanel::update_storage_buttons() {
  const lv_color_t accent = lv_color_hex(powerui::COLOR_ACCENT);
  lv_obj_t *buttons[4] = {local_btn, usb_btn, timelapse_btn, history_btn};
  for (int i = 0; i < 4; ++i) {
    const bool selected = i == view_mode;
    lv_obj_set_style_bg_color(buttons[i], selected ? accent : lv_color_hex(powerui::COLOR_SECONDARY), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(buttons[i], selected ? LV_OPA_20 : LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(buttons[i], selected ? accent : lv_color_hex(powerui::COLOR_WHITE), LV_PART_MAIN);
    lv_obj_set_style_border_opa(buttons[i], selected ? LV_OPA_50 : LV_OPA_10, LV_PART_MAIN);
    lv_obj_set_style_text_color(buttons[i], selected ? accent : lv_color_hex(powerui::COLOR_WHITE), LV_PART_MAIN);
  }
}

void PrintPanel::set_storage(bool usb) {
  usb_view = usb;
  view_mode = usb ? 1 : 0;
  cur_dir = &root;
  extra_view->hide();
  show_file_widgets(true);
  update_storage_buttons();
  subscribe();
}

void PrintPanel::show_file_widgets(bool visible) {
  lv_obj_t *widgets[] = {file_table_btns, file_grid, file_view};
  for (lv_obj_t *w : widgets) {
    if (visible) {
      lv_obj_clear_flag(w, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(w, LV_OBJ_FLAG_HIDDEN);
    }
  }
}

void PrintPanel::show_extra(FilesExtraView::Mode mode) {
  view_mode = mode == FilesExtraView::Mode::Timelapse ? 2 : 3;
  hide_delete_context();
  hide_delete_confirmation();
  show_file_widgets(false);
  update_storage_buttons();
  extra_view->show(mode);
}

// Make the USB drive visible to Moonraker: a "USB" link inside the gcodes folder pointing to the mounted drive.
// The link is removed when the drive is not mounted.
void PrintPanel::sync_usb_link() {
  try {
    const std::string gcodes = KUtils::get_root_path("gcodes");
    if (gcodes.empty()) {
      return;
    }
    const fs::path link = fs::path(gcodes) / USB_DIR_NAME;
    const std::string mount = usb_mount_point();

    std::error_code ec;
    const bool link_exists = fs::is_symlink(fs::symlink_status(link, ec));
    if (mount.empty()) {
      if (link_exists) {
        fs::remove(link, ec);
      }
      return;
    }

    if (link_exists && fs::read_symlink(link, ec) == fs::path(mount)) {
      return;
    }
    if (link_exists) {
      fs::remove(link, ec);
    }
    fs::create_directory_symlink(fs::path(mount), link, ec);
    if (ec) {
      spdlog::warn("could not link the USB drive {} into {}: {}", mount, link.string(), ec.message());
    } else {
      spdlog::debug("linked USB drive {} as {}", mount, link.string());
    }
  } catch (const std::exception &error) {
    spdlog::warn("USB link failed: {}", error.what());
  }
}

void PrintPanel::update_sort_buttons() {
  const lv_color_t accent = lv_color_hex(powerui::COLOR_ACCENT);
  lv_obj_t *buttons[2] = {modified_sort_btn, az_sort_btn};
  for (int i = 0; i < 2; ++i) {
    const bool selected = (i == 0) == sort_modified;
    lv_obj_set_style_bg_color(buttons[i], selected ? accent : lv_color_hex(powerui::COLOR_SECONDARY), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(buttons[i], selected ? LV_OPA_20 : LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(buttons[i], 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(buttons[i], selected ? accent : lv_color_hex(powerui::COLOR_WHITE), LV_PART_MAIN);
    lv_obj_set_style_border_opa(buttons[i], selected ? LV_OPA_50 : LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_text_color(buttons[i], selected ? accent : lv_color_hex(powerui::COLOR_FG), LV_PART_MAIN);
  }
}

// ---------------------------------------------------------------------------------------------------------------
// Start print dialog

struct PrintPanel::StartDialog {
  PrintPanel *panel;
  std::string path;
  bool has_mesh;
  bool has_purge;
  int current_mesh;
  bool current_purge;
  lv_obj_t *overlay;
  lv_obj_t *mesh_selector;
  lv_obj_t *purge_switch;
};

namespace {
const char *MESH_MAP[] = {"Off", "Adaptive", "Full", ""};
}

void PrintPanel::begin_print_flow(const std::string &path) {
  State *s = State::get_instance();
  auto adaptive = s->get_data("/printer_state/output_pin ADAPTIVE_BED_MESH/value"_json_pointer);
  auto full = s->get_data("/printer_state/output_pin FULL_BED_MESH/value"_json_pointer);
  auto purge = s->get_data("/printer_state/output_pin ADAPTIVE_PURGE_LINE/value"_json_pointer);
  const bool has_mesh = adaptive.is_number() && full.is_number();
  const bool has_purge = purge.is_number();
  if (!has_mesh && !has_purge) {
    start_print_now(path, "");
    return;
  }
  int mesh_mode = 0;
  if (has_mesh) {
    mesh_mode = adaptive.template get<double>() > 0.5 ? 1 : (full.template get<double>() > 0.5 ? 2 : 0);
  }
  show_start_dialog(path, has_mesh, mesh_mode, has_purge, has_purge && purge.template get<double>() > 0.5);
}

void PrintPanel::start_print_now(const std::string &path, const std::string &prepare_script) {
  auto start = [this, path]() {
    ws.send_jsonrpc("printer.print.start", json{{"filename", path}});
    print_status.foreground();
  };
  if (prepare_script.empty()) {
    start();
    return;
  }
  // Set the pins first; the print starts once Klipper has run them.
  ws.send_jsonrpc("printer.gcode.script", json{{"script", prepare_script}}, [this, start](json &) {
    std::lock_guard<std::mutex> lock(lv_lock);
    start();
  });
}

void PrintPanel::show_start_dialog(const std::string &path, bool has_mesh, int mesh_mode, bool has_purge, bool purge_on) {
  using namespace powerui;
  StartDialog *dialog = new StartDialog{this, path, has_mesh, has_purge, mesh_mode, purge_on, NULL, NULL, NULL};

  lv_obj_t *overlay = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(overlay);
  lv_obj_set_size(overlay, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(overlay, lv_color_hex(COLOR_BLACK), 0);
  lv_obj_set_style_bg_opa(overlay, LV_OPA_60, 0);
  lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_user_data(overlay, dialog);
  dialog->overlay = overlay;
  lv_obj_add_event_cb(overlay, [](lv_event_t *e) { delete (StartDialog *)lv_obj_get_user_data(lv_event_get_target(e)); },
                      LV_EVENT_DELETE, NULL);

  const int rows = (has_mesh ? 1 : 0) + (has_purge ? 1 : 0);
  const int w = 540, h = 150 + rows * 62 + 70;
  lv_obj_t *dlg = card(overlay, 0, 0, w, h);
  lv_obj_center(dlg);
  lv_obj_set_style_border_opa(dlg, LV_OPA_30, 0);

  lv_obj_t *title = label(dlg, "Start print", &lv_font_montserrat_20, lv_color_hex(COLOR_FG));
  lv_obj_set_pos(title, px(24), px(18));

  // File card.
  std::string name = path.substr(path.find_last_of('/') == std::string::npos ? 0 : path.find_last_of('/') + 1);
  lv_obj_t *file_card = plain(dlg);
  lv_obj_set_size(file_card, px(w - 48), px(56));
  lv_obj_set_pos(file_card, px(24), px(56));
  lv_obj_set_style_radius(file_card, px(10), 0);
  lv_obj_set_style_bg_color(file_card, lv_color_hex(COLOR_SECONDARY), 0);
  lv_obj_set_style_bg_opa(file_card, LV_OPA_COVER, 0);
  lv_obj_t *file_label = label(file_card, name.c_str(), &lv_font_montserrat_16, lv_color_hex(COLOR_FG));
  lv_label_set_long_mode(file_label, LV_LABEL_LONG_DOT);
  lv_obj_set_width(file_label, px(w - 48 - 28));
  lv_obj_align(file_label, LV_ALIGN_LEFT_MID, px(14), 0);

  int y = 126;
  auto row_title = [&](const char *text, const char *sub) {
    lv_obj_t *t = label(dlg, text, &lv_font_montserrat_16, lv_color_hex(COLOR_FG));
    lv_obj_set_pos(t, px(24), px(y + 6));
    lv_obj_t *s = label(dlg, sub, &lv_font_montserrat_12, lv_color_hex(COLOR_MUTED));
    lv_obj_set_pos(s, px(24), px(y + 30));
  };
  if (has_mesh) {
    row_title("Bed mesh", "Adaptive meshes only the area of the part");
    dialog->mesh_selector = lv_btnmatrix_create(dlg);
    lv_btnmatrix_set_map(dialog->mesh_selector, MESH_MAP);
    lv_btnmatrix_set_btn_ctrl_all(dialog->mesh_selector, LV_BTNMATRIX_CTRL_CHECKABLE);
    lv_btnmatrix_set_one_checked(dialog->mesh_selector, true);
    lv_btnmatrix_set_btn_ctrl(dialog->mesh_selector, mesh_mode, LV_BTNMATRIX_CTRL_CHECKED);
    lv_obj_set_size(dialog->mesh_selector, px(250), px(44));
    lv_obj_set_pos(dialog->mesh_selector, px(w - 24 - 250), px(y + 4));
    style_segmented(dialog->mesh_selector);
    lv_obj_set_style_text_font(dialog->mesh_selector, &lv_font_montserrat_14, LV_PART_ITEMS);
    y += 62;
  }
  if (has_purge) {
    row_title("Adaptive purge line", "Purges next to the part");
    dialog->purge_switch = lv_switch_create(dlg);
    style_switch(dialog->purge_switch);
    lv_obj_set_pos(dialog->purge_switch, px(w - 24 - 46), px(y + 14));
    if (purge_on) {
      lv_obj_add_state(dialog->purge_switch, LV_STATE_CHECKED);
    }
    y += 62;
  }

  action_button(dlg, NULL, "Cancel", ActionKind::Outline, 24, h - 74, 196, 50, &PrintPanel::_start_dialog_event, dialog);
  action_button(dlg, &ui_icon_play, "Start", ActionKind::Primary, w - 24 - 292, h - 74, 292, 50, &PrintPanel::_start_dialog_event, dialog);
}

void PrintPanel::_start_dialog_event(lv_event_t *event) {
  StartDialog *dialog = (StartDialog *)event->user_data;
  lv_obj_t *btn = lv_event_get_current_target(event);
  // Child order of the dialog card: Cancel is followed by Start (the last two children).
  lv_obj_t *card_obj = lv_obj_get_parent(btn);
  const uint32_t count = lv_obj_get_child_cnt(card_obj);
  const bool start = btn == lv_obj_get_child(card_obj, count - 1);

  PrintPanel *panel = dialog->panel;
  const std::string path = dialog->path;
  std::string script;
  if (start) {
    if (dialog->has_mesh) {
      const int mode = lv_btnmatrix_get_selected_btn(dialog->mesh_selector);
      const int checked = lv_btnmatrix_has_btn_ctrl(dialog->mesh_selector, 0, LV_BTNMATRIX_CTRL_CHECKED) ? 0
                        : lv_btnmatrix_has_btn_ctrl(dialog->mesh_selector, 1, LV_BTNMATRIX_CTRL_CHECKED) ? 1 : 2;
      (void)mode;
      if (checked != dialog->current_mesh) {
        script += checked == 0 ? "_BED_MESH_OFF\n" : (checked == 1 ? "_ADAPTIVE_BED_MESH_ON\n" : "_FULL_BED_MESH_ON\n");
      }
    }
    if (dialog->has_purge) {
      const bool on = lv_obj_has_state(dialog->purge_switch, LV_STATE_CHECKED);
      if (on != dialog->current_purge) {
        script += on ? "_ADAPTIVE_PURGE_LINE_ON\n" : "_ADAPTIVE_PURGE_LINE_OFF\n";
      }
    }
    if (!script.empty()) {
      script.pop_back();
    }
  }
  lv_obj_del_async(dialog->overlay);
  if (start) {
    panel->start_print_now(path, script);
  }
}
