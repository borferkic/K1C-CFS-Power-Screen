#include "prompt_panel.h"
#include "powerui.h"
#include "state.h"
#include "utils.h"
#include "spdlog/spdlog.h"

#include <cstring>

LV_IMG_DECLARE(load_filament_img);
LV_IMG_DECLARE(unload_filament_img);
LV_IMG_DECLARE(resume);
LV_IMG_DECLARE(cancel);

// uncomment for helper boxes
// #define DEBUG_LINES

static lv_style_t style_btn_grey;
static lv_style_t style_btn_blue;
static lv_style_t style_btn_green;
static lv_style_t style_btn_red;
static lv_style_t style_btn_orange;
static lv_style_t style_btn_dark_grey;
static lv_style_t button_group_flex_style;

PromptPanel::PromptPanel(KWebSocketClient &websocket_client, std::mutex &lock, lv_obj_t *parent)
    : NotifyConsumer(lock)
    , ws(websocket_client)
    , prompt_cont(lv_obj_create(lv_scr_act()))
    , flex(lv_obj_create(prompt_cont))
    , header(lv_label_create(prompt_cont))
    , footer_cont(lv_obj_create(prompt_cont))
//  , back_btn(promptpanel_cont, &back, "Back", &PromptPanel::_handle_callback, this)
{
    lv_obj_set_style_pad_all(prompt_cont, 0, 0);
    
    // lv_obj_clear_flag(promptpanel_cont, LV_OBJ_FLAG_SCROLLABLE);

    static lv_coord_t grid_main_row_dsc_detail[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    // header, flex, buttons
    static lv_coord_t grid_main_col_dsc_detail[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST}; 
    // single column

    lv_obj_center(prompt_cont);
    lv_obj_set_style_pad_all(prompt_cont, 16, 0);
    lv_obj_set_style_radius(prompt_cont, 14, LV_PART_MAIN);
    lv_obj_set_style_bg_color(prompt_cont, lv_color_hex(powerui::COLOR_CARD), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(prompt_cont, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(prompt_cont, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(prompt_cont, lv_color_hex(powerui::COLOR_WHITE), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(prompt_cont, LV_OPA_30, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(prompt_cont, 0, LV_PART_MAIN);
    lv_obj_set_style_text_color(prompt_cont, lv_color_hex(powerui::COLOR_FG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(flex, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(flex, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(footer_cont, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(footer_cont, 0, LV_PART_MAIN);
    lv_label_set_recolor(header, true);
    lv_obj_set_style_max_height(prompt_cont, lv_pct(90), 0);
    lv_obj_set_style_max_width(prompt_cont, lv_pct(75), 0);
    lv_obj_set_style_min_height(prompt_cont, lv_pct(60), 0);
    lv_obj_set_style_min_width(prompt_cont, lv_pct(60), 0);
    lv_obj_set_size(prompt_cont, lv_pct(72), lv_pct(60));
    lv_obj_set_grid_dsc_array(prompt_cont, grid_main_col_dsc_detail, grid_main_row_dsc_detail);

    lv_obj_set_style_pad_all(flex, 0, 0);

    lv_obj_set_grid_cell(header,                LV_GRID_ALIGN_START,    0, 1, LV_GRID_ALIGN_START,  0, 1);
    lv_obj_set_grid_cell(flex,                  LV_GRID_ALIGN_START,    0, 1, LV_GRID_ALIGN_CENTER, 1, 1);
    lv_obj_set_grid_cell(footer_cont,          LV_GRID_ALIGN_CENTER,   0, 1, LV_GRID_ALIGN_END,    2, 1);

    lv_obj_set_size(header, lv_pct(100), lv_pct(10));
    lv_obj_set_size(flex, lv_pct(100), lv_pct(60));
    lv_obj_set_size(footer_cont, lv_pct(100), lv_pct(15));
    lv_obj_set_style_text_align(header, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(header, &lv_font_montserrat_20, 0);

    lv_obj_clear_flag(prompt_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(flex, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(footer_cont, LV_OBJ_FLAG_SCROLLABLE);


    // set buttons horizontal
    //
    lv_style_init(&button_group_flex_style);
    lv_style_set_flex_flow(&button_group_flex_style, LV_FLEX_FLOW_ROW_WRAP);
    lv_style_set_flex_main_place(&button_group_flex_style, LV_FLEX_ALIGN_SPACE_EVENLY);
    lv_style_set_flex_cross_place(&button_group_flex_style, LV_FLEX_ALIGN_CENTER);
    lv_style_set_flex_track_place(&button_group_flex_style, LV_FLEX_ALIGN_CENTER);
    lv_style_set_layout(&button_group_flex_style, LV_LAYOUT_FLEX);
    lv_style_set_pad_all(&button_group_flex_style, 0);
    lv_style_set_pad_row(&button_group_flex_style, 10);
    lv_style_set_pad_column(&button_group_flex_style, 10);
    lv_style_set_height(&button_group_flex_style, LV_SIZE_CONTENT);
    lv_style_set_width(&button_group_flex_style, lv_pct(100));
    lv_style_set_outline_pad(&button_group_flex_style, 0);
    lv_style_set_outline_width(&button_group_flex_style, 0);
    lv_obj_add_style(footer_cont, &button_group_flex_style, 0);

    static lv_style_t flex_style;
    lv_style_init(&flex_style);
    lv_style_set_flex_flow(&flex_style, LV_FLEX_FLOW_COLUMN_WRAP);
    lv_style_set_flex_main_place(&flex_style, LV_FLEX_ALIGN_SPACE_EVENLY);
    lv_style_set_flex_cross_place(&flex_style, LV_FLEX_ALIGN_CENTER);
    lv_style_set_flex_track_place(&flex_style, LV_FLEX_ALIGN_CENTER);
    lv_style_set_layout(&flex_style, LV_LAYOUT_FLEX);
    lv_style_set_pad_all(&flex_style, 0);
    lv_style_set_outline_pad(&flex_style, 0);
    lv_style_set_outline_width(&flex_style, 0);
    lv_obj_add_style(flex, &flex_style, 0);

#ifdef DEBUG_LINES
    // for debugging
    lv_obj_set_style_border_width(header, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(header, lv_color_hex(powerui::COLOR_MAT_RED), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(flex, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(flex, lv_color_hex(powerui::COLOR_MAT_YELLOW), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(footer_cont, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(footer_cont, lv_color_hex(powerui::COLOR_MAT_BLUE), LV_PART_MAIN | LV_STATE_DEFAULT);
#endif

    ws.register_notify_update(this);
    ws.register_method_callback("notify_gcode_response", "MainPanel",[this](json& d) { this->handle_macro_response(d); });

    // create header
    lv_label_set_text(header, "HEADER");

    // button styles
    lv_style_init(&style_btn_grey);
    lv_style_set_bg_color(&style_btn_grey, lv_color_hex(powerui::COLOR_MAT_GREY));
    lv_style_set_bg_opa(&style_btn_grey, LV_OPA_COVER);
    lv_style_set_pad_all(&style_btn_grey, 0);

    lv_style_init(&style_btn_blue);
    lv_style_set_bg_color(&style_btn_blue, lv_color_hex(powerui::COLOR_MAT_BLUE));
    lv_style_set_bg_opa(&style_btn_blue, LV_OPA_COVER);

    lv_style_init(&style_btn_green);
    lv_style_set_bg_color(&style_btn_green, lv_color_hex(powerui::COLOR_MAT_GREEN));
    lv_style_set_bg_opa(&style_btn_green, LV_OPA_COVER);

    lv_style_init(&style_btn_red);
    lv_style_set_bg_color(&style_btn_red, lv_color_hex(powerui::COLOR_MAT_RED));
    lv_style_set_bg_opa(&style_btn_red, LV_OPA_COVER);

    lv_style_init(&style_btn_orange);
    lv_style_set_bg_color(&style_btn_orange, lv_color_hex(powerui::COLOR_MAT_ORANGE));
    lv_style_set_bg_opa(&style_btn_orange, LV_OPA_COVER);

    lv_style_init(&style_btn_dark_grey);
    lv_style_set_bg_color(&style_btn_dark_grey, lv_color_hex(powerui::COLOR_MAT_GREY_DARK));
    lv_style_set_bg_opa(&style_btn_dark_grey, LV_OPA_COVER);
    
    background(); // hide ourselves
}


// LOAD/UNLOAD move and heat the extruder, so they stay disabled while a print
// is running. They are enabled while paused (M600 or PAUSE) or idle.
void PromptPanel::refresh_filament_buttons() {
    auto &pstate = State::get_instance()->get_data("/printer_state/print_stats/state"_json_pointer);
    const bool printing = pstate.is_string() && pstate.template get<std::string>() == "printing";
    for (lv_obj_t *btn : {load_btn, unload_btn}) {
        if (btn == NULL) {
            continue;
        }
        if (printing) {
            lv_obj_add_state(btn, LV_STATE_DISABLED);
            lv_obj_set_style_opa(btn, LV_OPA_40, 0);
        } else {
            lv_obj_clear_state(btn, LV_STATE_DISABLED);
            lv_obj_set_style_opa(btn, LV_OPA_COVER, 0);
        }
    }
}

void PromptPanel::consume(json &j) {
    auto &pstate = j["/params/0/print_stats/state"_json_pointer];
    if (pstate.is_null()) {
        return;
    }
    std::lock_guard<std::mutex> lock(lv_lock);
    refresh_filament_buttons();
}

PromptPanel::~PromptPanel() {
    if (prompt_cont != NULL) {
        lv_obj_del(prompt_cont);
        prompt_cont = NULL;
    }

    ws.unregister_notify_update(this);
}

void PromptPanel::foreground() {
    // shrink wrap
    lv_obj_clear_flag(prompt_cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(prompt_cont);
}

void PromptPanel::background() {
  lv_obj_add_flag(prompt_cont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_background(prompt_cont);
}

void PromptPanel::handle_callback(lv_event_t *event) {
    lv_obj_t *btn = lv_event_get_current_target(event);

    lv_obj_t *label = lv_obj_get_child(btn, 0);
    lv_obj_t *command = lv_obj_get_child(btn, 1);
    // check if btn in command map
    spdlog::debug("handle event");

    if (btn == NULL) {
        spdlog::debug("no button found");
    }

    if (label != NULL) {
        spdlog::debug("button: {}", lv_label_get_text(label));
    }

    if (command != NULL) {
        std::string cmd = lv_label_get_text(command);
        spdlog::debug("button: {}", cmd);

        // The Manual M600 screen uses these terminal actions. Hide the
        // native prompt before running them so no nested RESPOND is needed.
        const char *button_text = label == NULL ? "" : lv_label_get_text(label);
        if (!strcmp(button_text, "RESUME") || !strcmp(button_text, "STOP") || !strcmp(button_text, "CLOSE") || !strcmp(button_text, "X")) {
            background();
        }

        ws.gcode_script(cmd);
    }


}

void PromptPanel::check_height() {
    // check if we need to increase size of the parent container
    lv_obj_t *last_child = lv_obj_get_child(flex, -1);
    // iterate max 5 times to enlarge, could probably be done nicer but since it's 
    // based on css auto sizing is terrible.
    if (NULL != last_child) {
        int count = 0;
        int y = lv_obj_get_y(last_child);
        int height = lv_obj_get_height(last_child);
        while(((y + height > lv_obj_get_height(flex)) || height == 0) && count < 5) {
            spdlog::debug("y: {}, h: {}", y, height);
            if ((y + height > lv_obj_get_height(flex)) || height == 0) {
                int newheight = (int) (((double)lv_obj_get_height(prompt_cont)) * 1.1);
                int newwidth = (int) (((double)lv_obj_get_width(prompt_cont)) * 1.1);
                spdlog::debug("Increase size of panel: {}, {}", newheight, newwidth);
                lv_obj_set_size(prompt_cont, newheight, newwidth);
            }
            lv_obj_update_layout(prompt_cont);
            count++;
            y = lv_obj_get_y(last_child);
            height = lv_obj_get_height(last_child);
        }
    }
}

void PromptPanel::handle_macro_response(json &j) {
    spdlog::trace("macro response: {}", j.dump());
    auto &v = j["/params/0"_json_pointer];

    if (!v.is_null()) {
        spdlog::debug("data found");
        std::string resp = v.template get<std::string>();
        std::lock_guard<std::mutex> lock(lv_lock);
        spdlog::debug("data: {}", resp);

        if (resp.find("// action:", 0) == 0) {
            // it is an action
            std::string command = resp.substr(10);
            spdlog::debug("action: {}", command);

            
            if (command.find("prompt_begin") == 0) {
                std::string prompt_header = command.substr(13);
                spdlog::debug("PROMPT_BEGIN: {}", prompt_header);

                manual_filament_prompt = prompt_header == "MANUAL FILAMENT CHANGE";
                manual_button_count = 0;
                load_btn = NULL;
                unload_btn = NULL;

                // remove buttons
                lv_obj_clean(footer_cont);
                lv_obj_clean(flex);
                prompt_has_text = false;
                lv_obj_add_flag(flex, LV_OBJ_FLAG_HIDDEN);
                lv_obj_set_grid_cell(footer_cont, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 1, 2);
                lv_obj_set_size(footer_cont, lv_pct(100), lv_pct(100));
                if (manual_filament_prompt) {
                    static lv_coord_t manual_button_cols[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                    // Two compact rows: LOAD, UNLOAD, RESUME above; STOP and CLOSE below.
                    static lv_coord_t manual_button_rows[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
                    lv_obj_set_layout(footer_cont, LV_LAYOUT_GRID);
                    lv_obj_set_grid_dsc_array(footer_cont, manual_button_cols, manual_button_rows);
                    // Keep the 2x2 controls inside the K1C display margins.
                    lv_obj_set_style_pad_all(footer_cont, 0, 0);
                    // Leave a clear gap after the instruction and between both rows.
                    lv_obj_set_style_pad_top(footer_cont, 8, 0);
                    lv_obj_set_style_pad_row(footer_cont, 12, 0);
                    lv_obj_set_style_pad_column(footer_cont, 12, 0);
                } else {
                    lv_obj_set_layout(footer_cont, LV_LAYOUT_FLEX);
                    lv_obj_set_flex_flow(footer_cont, LV_FLEX_FLOW_ROW_WRAP);
                    lv_obj_set_style_pad_all(footer_cont, 0, 0);
                    lv_obj_set_style_pad_row(footer_cont, 10, 0);
                    lv_obj_set_style_pad_column(footer_cont, 10, 0);
                }
                if (close_btn != NULL) {
                    lv_obj_del(close_btn);
                    close_btn = NULL;
                }
                // remove button commands

                if (manual_filament_prompt) {
                    // The K1C needs extra vertical room for the second row.
                    lv_obj_set_style_min_width(prompt_cont, 0, 0);
                    lv_obj_set_style_min_height(prompt_cont, 0, 0);
                    lv_obj_set_size(prompt_cont, powerui::px(392), powerui::px(322));
                } else {
                    lv_obj_set_style_min_height(prompt_cont, lv_pct(60), 0);
                    lv_obj_set_style_min_width(prompt_cont, lv_pct(60), 0);
                    lv_obj_set_size(prompt_cont, lv_pct(72), lv_pct(60));
                }
                lv_obj_set_height(flex, lv_pct(70));

                // set header here
                if (manual_filament_prompt) {
                    lv_label_set_text(header, "Manual filament change\n#A1A1A1 Select an option to continue#");
                    lv_obj_set_height(header, LV_SIZE_CONTENT);
                } else {
                    lv_label_set_text(header, prompt_header.c_str());
                }
            } else if (command.find("prompt_text") == 0) {
                std::string prompt_text = command.substr(12);
                spdlog::debug("PROMPT_TEXT: {}", prompt_text);
                prompt_has_text = true;
                lv_obj_clear_flag(flex, LV_OBJ_FLAG_HIDDEN);
                lv_obj_set_grid_cell(footer_cont, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_END, 2, 1);
                lv_obj_set_size(footer_cont, lv_pct(100), lv_pct(15));
                // create label and add to flex field
                lv_obj_t *textfield = lv_label_create(flex);
                lv_obj_set_width(textfield, lv_pct(96));
                lv_obj_set_height(textfield, 40);
                // lv_obj_set_style_min_height(textfield, 32, 0);
                lv_label_set_long_mode(textfield, LV_LABEL_LONG_WRAP);
                lv_obj_set_flex_grow(textfield, 1);
                lv_obj_set_style_outline_pad(textfield, 0, 0);
                lv_label_set_text(textfield, prompt_text.c_str());
                lv_obj_set_style_text_color(textfield, lv_color_hex(powerui::COLOR_FG), LV_PART_MAIN);
                lv_obj_center(textfield);
#ifdef DEBUG_LINES
                lv_obj_set_style_border_width(textfield, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_border_color(textfield, lv_color_hex(powerui::COLOR_MAT_GREEN), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_bg_color(textfield, lv_color_hex(powerui::COLOR_MAT_GREEN_LIGHT), LV_PART_MAIN | LV_STATE_DEFAULT);
#endif
            // due to using find, order IS important!  
            } else if (command.find("prompt_button_group_start") == 0) {
                spdlog::debug("Button group created");
                // create new button group in flex window and mark active
                button_group_cont = lv_obj_create(flex);
                lv_obj_add_style(button_group_cont, &button_group_flex_style, 0);
                lv_obj_set_flex_grow(button_group_cont, 1);
                lv_obj_set_width(button_group_cont, lv_pct(96));
                lv_obj_center(button_group_cont);
                lv_obj_set_style_pad_all(button_group_cont, 0, 0);
                lv_obj_set_style_outline_pad(button_group_cont, 0, 0);
                lv_obj_set_style_max_height(button_group_cont, lv_pct(62), 0);
                lv_obj_set_style_min_height(button_group_cont, 48, 0);
                lv_obj_set_height(button_group_cont, LV_SIZE_CONTENT);
                lv_obj_clear_flag(button_group_cont, LV_OBJ_FLAG_SCROLLABLE);

#ifdef DEBUG_LINES
                lv_obj_set_style_border_width(button_group_cont, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_border_color(button_group_cont, lv_color_hex(powerui::COLOR_MAT_PINK), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_bg_color(button_group_cont, lv_color_hex(powerui::COLOR_MAT_PINK_LIGHT), LV_PART_MAIN | LV_STATE_DEFAULT);
#endif
                // lv_obj_set_style_min_height(button_group_cont, lv_pct(5), 0);
            } else if (command.find("prompt_button_group_end") == 0) {
                // does nothing since start creates a new one
                spdlog::debug("Button group ended");
                button_group_cont = NULL;
            } else if (command.find("prompt_footer_button") == 0 || command.find("prompt_button") == 0) {
                int index_label = command.find("button", 0) + strlen("button");
                int index_first = command.find("|", index_label);
                int index_second = command.find("|", index_first + 1);
                spdlog::debug("indexes: {} {} {}", index_label, index_first, index_second);
                std::string prompt_footer_button = command.substr(index_label, index_first - index_label);
                const auto first_button_character = prompt_footer_button.find_first_not_of(" \t");
                if (first_button_character != std::string::npos) {
                    prompt_footer_button.erase(0, first_button_character);
                }
                std::string prompt_button_command;
                std::string prompt_button_type = "none";
                spdlog::debug("button: {} |  {} | {}", prompt_footer_button, prompt_button_command, prompt_button_type);
                if (index_second > 0) {
                    prompt_button_command = command.substr(index_first + 1, index_second - index_first - 1);
                    prompt_button_type = command.substr(index_second + 1, command.length() - index_second - 1);
                } else {
                    prompt_button_command = command.substr(index_first + 1);
                }
                spdlog::debug("PROMPT_FOOTER_BUTTON: {} CMD: {}, type {}", prompt_footer_button, prompt_button_command, prompt_button_type);
                lv_obj_t *btn = NULL;
                if (command.find("prompt_footer_button") == 0) {
                    btn = lv_btn_create(footer_cont);
                } else {
                    if (button_group_cont == NULL) {
                        btn = lv_btn_create(flex);
                    } else {
                        btn = lv_btn_create(button_group_cont);
                    }
                }
                if (btn) {
                    if (manual_filament_prompt) {
                        // LOAD and UNLOAD on the first row, RESUME and STOP on the second, a small CLOSE pill below.
                        int button_column = 0;
                        int button_row = 0;
                        if (prompt_footer_button == "UNLOAD") {
                            button_column = 1;
                        } else if (prompt_footer_button == "RESUME") {
                            button_row = 1;
                        } else if (prompt_footer_button == "STOP") {
                            button_column = 1;
                            button_row = 1;
                        } else if (prompt_footer_button == "CLOSE") {
                            button_row = 2;
                        }
                        if (prompt_footer_button == "CLOSE") {
                            lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_CENTER, 0, 2, LV_GRID_ALIGN_CENTER, button_row, 1);
                            lv_obj_set_size(btn, powerui::px(96), powerui::px(32));
                        } else {
                            lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_STRETCH, button_column, 1, LV_GRID_ALIGN_CENTER, button_row, 1);
                            lv_obj_set_size(btn, LV_PCT(100), powerui::px(84));
                        }
                        lv_obj_set_style_min_width(btn, 0, 0);
                        lv_obj_set_style_min_height(btn, 0, 0);
                    } else {
                        lv_obj_set_size(btn, lv_pct(46), 100);
                        lv_obj_set_style_max_width(btn, lv_pct(46), 0);
                        lv_obj_set_style_min_width(btn, 120, 0);
                        lv_obj_set_style_max_height(btn, 110, 0);
                        lv_obj_set_style_min_height(btn, 90, 0);
                    }
                    lv_obj_set_style_outline_pad(btn, 0, 0);
                    if (!manual_filament_prompt) {
                        lv_obj_center(btn);
                        lv_obj_set_flex_grow(btn, 1);
                    }
                    lv_obj_t *label = lv_label_create(btn);
                    // a hidden label is abused to transfer the command and auto-clean it
                    lv_obj_t *command = lv_label_create(btn);
                    lv_obj_set_size(command, 1, 1);
                    lv_obj_add_flag(command, LV_OBJ_FLAG_HIDDEN);
                    lv_label_set_text(label, prompt_footer_button.c_str());
                    lv_label_set_text(command, prompt_button_command.c_str());
                    lv_obj_set_style_pad_all(btn, 2, 0);
                    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
                    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);

                    const void *button_icon = NULL;
                    if (prompt_footer_button == "LOAD") {
                        button_icon = &load_filament_img;
                    } else if (prompt_footer_button == "UNLOAD") {
                        button_icon = &unload_filament_img;
                    } else if (prompt_footer_button == "RESUME") {
                        button_icon = &resume;
                    } else if (prompt_footer_button == "STOP") {
                        button_icon = &cancel;
                    }
                    if (button_icon != NULL) {
                        lv_obj_t *icon = lv_img_create(btn);
                        lv_img_set_src(icon, button_icon);
                        lv_img_set_size_mode(icon, LV_IMG_SIZE_MODE_REAL);
                        const lv_img_dsc_t *icon_dsc = static_cast<const lv_img_dsc_t *>(button_icon);
                        const int icon_natural = icon_dsc->header.w > icon_dsc->header.h ? icon_dsc->header.w : icon_dsc->header.h;
                        lv_img_set_zoom(icon, 256 * 28 / (icon_natural > 0 ? icon_natural : 64));  // 28 px, like the other button icons of the theme
                        lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 10);
                        lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -10);
                    } else {
                        lv_obj_center(label);
                    }

                    if (manual_filament_prompt && prompt_footer_button == "LOAD") {
                        load_btn = btn;
                    } else if (manual_filament_prompt && prompt_footer_button == "UNLOAD") {
                        unload_btn = btn;
                    }

                    {
                        // PowerUI kinds: success is the filled green action, error the destructive one, the rest are outlined.
                        lv_color_t main_color = lv_color_hex(powerui::COLOR_FG);
                        lv_color_t bg = lv_color_hex(powerui::COLOR_CARD);
                        lv_opa_t border_opa = LV_OPA_10;
                        lv_color_t border = lv_color_hex(powerui::COLOR_WHITE);
                        lv_color_t pressed = lv_color_hex(powerui::COLOR_SECONDARY);
                        int border_width = 1;
                        if (!prompt_button_type.compare("success")) {
                            main_color = lv_color_hex(powerui::COLOR_WHITE);
                            bg = lv_color_hex(powerui::COLOR_PRIMARY);
                            pressed = lv_color_hex(powerui::COLOR_PRIMARY_PRESSED);
                            border_width = 0;
                        } else if (!prompt_button_type.compare("error")) {
                            main_color = lv_color_hex(powerui::COLOR_DESTRUCTIVE);
                            border = main_color;
                            border_opa = LV_OPA_40;
                        }
                        lv_obj_set_style_bg_color(btn, bg, LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(btn, pressed, LV_PART_MAIN | LV_STATE_PRESSED);
                        lv_obj_set_style_radius(btn, 10, LV_PART_MAIN);
                        lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
                        lv_obj_set_style_border_width(btn, border_width, LV_PART_MAIN);
                        lv_obj_set_style_border_color(btn, border, LV_PART_MAIN);
                        lv_obj_set_style_border_opa(btn, border_opa, LV_PART_MAIN);
                        lv_obj_set_style_text_color(btn, main_color, LV_PART_MAIN);
                        lv_obj_set_style_text_font(label, &lv_font_montserrat_16, LV_PART_MAIN);
                        if (manual_filament_prompt && prompt_footer_button == "CLOSE") {
                            lv_obj_set_style_bg_color(btn, lv_color_hex(powerui::COLOR_PRIMARY), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(btn, lv_color_hex(powerui::COLOR_PRIMARY_PRESSED), LV_PART_MAIN | LV_STATE_PRESSED);
                            lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
                            lv_obj_set_style_radius(btn, powerui::px(8), LV_PART_MAIN);
                            lv_obj_set_style_text_color(btn, lv_color_hex(powerui::COLOR_WHITE), LV_PART_MAIN);
                            lv_label_set_text(label, LV_SYMBOL_CLOSE "  Close");
                        }
                        if (button_icon != NULL) {
                            lv_obj_t *icon_obj = lv_obj_get_child(btn, lv_obj_get_child_cnt(btn) - 1);
                            lv_obj_set_style_img_recolor(icon_obj, main_color, LV_PART_MAIN);
                            lv_obj_set_style_img_recolor_opa(icon_obj, LV_OPA_COVER, LV_PART_MAIN);
                        }
                    }
                    lv_obj_add_event_cb(btn, _handle_callback, LV_EVENT_PRESSED, this);

                }
            } else if (command.find("prompt_show") == 0) {
                spdlog::debug("PROMPT_SHOW");
                refresh_filament_buttons();
                check_height();
                foreground();
            } else if (command.find("prompt_end") == 0) {
                spdlog::debug("PROMPT_END");
                background();

                // remove buttons
                lv_obj_clean(footer_cont);
                lv_obj_clean(flex);
                prompt_has_text = false;
                manual_filament_prompt = false;
                manual_button_count = 0;
                load_btn = NULL;
                unload_btn = NULL;
                lv_obj_clear_flag(flex, LV_OBJ_FLAG_HIDDEN);
                lv_obj_set_grid_cell(footer_cont, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_END, 2, 1);
                lv_obj_set_size(footer_cont, lv_pct(100), lv_pct(15));
                if (close_btn != NULL) {
                    lv_obj_del(close_btn);
                    close_btn = NULL;
                }
                lv_obj_set_size(prompt_cont, lv_pct(60), lv_pct(50));
                lv_obj_set_height(flex, LV_SIZE_CONTENT);
            } else {
                spdlog::debug("action {} --- not supported", command);
            }
            
        }
        
    }
}
