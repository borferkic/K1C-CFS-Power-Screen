#ifndef __POWERUI_H__
#define __POWERUI_H__

#include "lvgl/lvgl.h"

#include <functional>
#include <string>

// PowerUI theme helpers: shared colors and small widget builders used by the new Home screen.
// All sizes are given in design pixels for an 800x480 display and scaled by px().
namespace powerui {

constexpr uint32_t COLOR_BG = 0x0A0A0A;
constexpr uint32_t COLOR_CARD = 0x171717;
constexpr uint32_t COLOR_SECONDARY = 0x262626;
constexpr uint32_t COLOR_FG = 0xFAFAFA;
constexpr uint32_t COLOR_MUTED = 0xA1A1A1;
constexpr uint32_t COLOR_ACCENT = 0x4ADE80;
constexpr uint32_t COLOR_ACCENT_DARK = 0x052E16;
// Filled green action buttons (shadcn "default" variant) and their pressed state.
constexpr uint32_t COLOR_PRIMARY = 0x16A34A;
constexpr uint32_t COLOR_PRIMARY_PRESSED = 0x15803D;
// Secondary color of the LVGL base theme (the accent is fixed: PowerUI has no selectable themes).
constexpr uint32_t COLOR_DANGER = 0xF44336;          // solid red: emergency stop, confirm-restart
constexpr uint32_t COLOR_LVGL_SECONDARY = COLOR_DANGER;
constexpr uint32_t COLOR_ACCENT_PRESSED = 0x22C55E;
constexpr uint32_t COLOR_ACCENT_BG = 0x0F1F15;       // tinted green surface (success notice)
constexpr uint32_t COLOR_DESTRUCTIVE_BG = 0x2A1517;  // tinted red surface (failure)
constexpr uint32_t COLOR_PRESSED = 0x333333;         // pressed state of the secondary buttons
constexpr uint32_t COLOR_NEUTRAL = 0x3A3A3A;         // neutral cell of the bed mesh
constexpr uint32_t COLOR_STEP_TODO = 0x404040;       // pending step dot of the update popup
constexpr uint32_t COLOR_WHITE = 0xFFFFFF;           // borders (with opacity) and text on filled buttons
constexpr uint32_t COLOR_BLACK = 0x000000;
// Material palette colors kept for the generic Klipper prompt dialog, the TMC chart and the legacy dialogs.
constexpr uint32_t COLOR_MAT_RED = 0xF44336;
constexpr uint32_t COLOR_MAT_RED_DARK = 0xD32F2F;
constexpr uint32_t COLOR_MAT_PINK = 0xE91E63;
constexpr uint32_t COLOR_MAT_PINK_LIGHT = 0xF06292;
constexpr uint32_t COLOR_MAT_BLUE = 0x2196F3;
constexpr uint32_t COLOR_MAT_GREEN = 0x4CAF50;
constexpr uint32_t COLOR_MAT_GREEN_LIGHT = 0x81C784;
constexpr uint32_t COLOR_MAT_YELLOW = 0xFFEB3B;
constexpr uint32_t COLOR_MAT_ORANGE = 0xFF9800;
constexpr uint32_t COLOR_MAT_GREY = 0x9E9E9E;
constexpr uint32_t COLOR_MAT_GREY_DARK = 0x757575;
constexpr uint32_t COLOR_DESTRUCTIVE = 0xFF6467;
constexpr uint32_t COLOR_WARNING = 0xFB923C;
constexpr uint32_t COLOR_EXTRUDER = 0xF87171;
constexpr uint32_t COLOR_BED = 0xC084FC;
constexpr uint32_t COLOR_CHAMBER = 0x60A5FA;

double scale();
lv_coord_t px(int design_px);

// Area left for the panels that open over the Home tabs: the screen minus the sidebar column (64) and the
// title bar (40). Panels size their fixed-pixel layouts with these instead of the full display resolution.
lv_coord_t overlay_width_px();
lv_coord_t overlay_height_px();
double overlay_width_scale();   // overlay width / 800
double overlay_height_scale();  // overlay height / 480

// Transparent, non-clickable, non-scrollable container without padding or border.
lv_obj_t *plain(lv_obj_t *parent);

// Card: rounded dark surface with a 1 px translucent border.
lv_obj_t *card(lv_obj_t *parent, int x, int y, int w, int h);

// Label with font and color.
lv_obj_t *label(lv_obj_t *parent, const char *text, const lv_font_t *font, lv_color_t color);

// Tinted image drawn to size_px (wrapper of exactly that size, image zoomed and centered inside).
lv_obj_t *icon(lv_obj_t *parent, const lv_img_dsc_t *src, int size_px, lv_color_t color);
void icon_set_color(lv_obj_t *wrapper, lv_color_t color);

// Empty state (shadcn "Empty"): icon tile, title and a muted description, centered in `parent`.
lv_obj_t *empty_state(lv_obj_t *parent, const lv_img_dsc_t *icon_src, const char *title, const char *description);

// Pill with a colored dot and a text (e.g. "Printing").
lv_obj_t *badge(lv_obj_t *parent, const char *text, lv_color_t dot);
void badge_set(lv_obj_t *badge, const char *text, lv_color_t dot);

// Two-button view switch: child 0 = print view, child 1 = temperature chart.
lv_obj_t *view_toggle(lv_obj_t *parent, lv_event_cb_t cb, void *user_data);
void view_toggle_set(lv_obj_t *toggle, bool print_view);

// Overlay panels (Filament, Homing, System...) announce themselves so the main title bar can show their title and a
// Back button. `on_back` runs when the Back button is touched. MainPanel installs the handlers.
void set_overlay_handlers(std::function<void(const std::string &, std::function<void()>)> open_handler);
void overlay_open(const std::string &title, std::function<void()> on_back);

// Styles for the existing icon-and-label buttons (ButtonContainer): `container` is the button's outer object and
// `inner` its image button (made transparent). Icon and text take the color of the kind.
enum class ButtonKind { Outline, Soft, Destructive };
void style_button(lv_obj_t *container, lv_obj_t *inner, ButtonKind kind);

// Dropdown (select) look: card surface with a thin border, rounded 10, muted arrow and a themed list.
void style_select(lv_obj_t *dropdown);

// Segmented control look for a button matrix with checkable items (soft green when selected).
void style_segmented(lv_obj_t *btnm);

// Slider look: translucent track, green indicator and a white round knob.
void style_slider(lv_obj_t *slider);

// Switch look (shadcn): grey track, green when on, round knob.
void style_switch(lv_obj_t *sw);

// Action button of the tool screens: optional line icon (a 32 px mask, tinted) and a text on one line.
// Primary = filled green, Outline = bordered card, Destructive = red outline. Returns the clickable button; `cb`
// receives LV_EVENT_CLICKED with `user_data`.
enum class ActionKind { Primary, Outline, Destructive };
lv_obj_t *action_button(lv_obj_t *parent, const lv_img_dsc_t *icon_src, const char *text, ActionKind kind,
                        int x, int y, int w, int h, lv_event_cb_t cb, void *user_data);

// Background of an overlay panel's root container: page color, no padding or border, not scrollable.
void style_overlay_root(lv_obj_t *cont);

} // namespace powerui

#endif // __POWERUI_H__
