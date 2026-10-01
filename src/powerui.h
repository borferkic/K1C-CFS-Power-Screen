#ifndef __POWERUI_H__
#define __POWERUI_H__

#include "lvgl/lvgl.h"

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

// Pill with a colored dot and a text (e.g. "Printing").
lv_obj_t *badge(lv_obj_t *parent, const char *text, lv_color_t dot);
void badge_set(lv_obj_t *badge, const char *text, lv_color_t dot);

// Two-button view switch: child 0 = print view, child 1 = temperature chart.
lv_obj_t *view_toggle(lv_obj_t *parent, lv_event_cb_t cb, void *user_data);
void view_toggle_set(lv_obj_t *toggle, bool print_view);

// Styles for the existing icon-and-label buttons (ButtonContainer): `container` is the button's outer object and
// `inner` its image button (made transparent). Icon and text take the color of the kind.
enum class ButtonKind { Outline, Soft, Destructive };
void style_button(lv_obj_t *container, lv_obj_t *inner, ButtonKind kind);

// Segmented control look for a button matrix with checkable items (soft green when selected).
void style_segmented(lv_obj_t *btnm);

} // namespace powerui

#endif // __POWERUI_H__
