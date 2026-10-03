#include "files_extra_view.h"
#include "powerui.h"
#include "state.h"
#include "config.h"
#include "utils.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cctype>
#include <utility>
#include <cmath>
#include <set>
#include <ctime>

LV_IMG_DECLARE(print);
LV_IMG_DECLARE(sd_img);
LV_IMG_DECLARE(ui_icon_camera);
LV_IMG_DECLARE(ui_icon_folder);
LV_IMG_DECLARE(ui_icon_trash);

using namespace powerui;

namespace {
std::string format_time(double epoch) {
  if (epoch <= 0) {
    return "-";
  }
  const std::time_t t = static_cast<std::time_t>(epoch);
  const std::tm tm = *std::localtime(&t);
  char buf[32] = {};
  std::strftime(buf, sizeof(buf), "%b %d %H:%M", &tm);
  return buf;
}

std::string format_duration(double seconds) {
  const long total = static_cast<long>(std::lround(seconds));
  const long h = total / 3600, m = (total % 3600) / 60;
  if (h > 0) {
    return fmt::format("{}h {:02d}m", h, m);
  }
  return fmt::format("{}m", m);
}

std::string format_size(double bytes) {
  if (bytes >= 1024.0 * 1024.0) {
    return fmt::format("{:.0f} MB", bytes / (1024.0 * 1024.0));
  }
  return fmt::format("{:.0f} KB", bytes / 1024.0);
}

std::string format_length(double mm) {
  const double m = mm / 1000.0;
  return m >= 1000.0 ? fmt::format("{:.1f} km", m / 1000.0) : fmt::format("{:.0f} m", m);
}

std::string base_name(const std::string &path) {
  const size_t slash = path.find_last_of('/');
  return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string strip_ext(const std::string &name) {
  const size_t dot = name.find_last_of('.');
  return dot == std::string::npos ? name : name.substr(0, dot);
}
}

FilesExtraView::FilesExtraView(KWebSocketClient &c, std::mutex &lock, lv_obj_t *left_parent, lv_obj_t *detail_parent,
                               std::function<void(const std::string &)> start)
  : ws(c)
  , lv_lock(lock)
  , start_print(std::move(start))
  , mode(Mode::History)
  , shown(false)
  , selected(-1)
  , toolbar(NULL)
  , stats_label(NULL)
  , toolbar_btn(NULL)
  , list(NULL)
  , detail(NULL)
  , primary_btn(NULL)
  , delete_btn(NULL)
{
  // Toolbar: summary text on the left and one square button (refresh or reset totals) on the right.
  toolbar = plain(left_parent);
  lv_obj_set_size(toolbar, LV_PCT(100), px(40));
  lv_obj_set_flex_flow(toolbar, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(toolbar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  stats_label = label(toolbar, "", &lv_font_montserrat_12, lv_color_hex(COLOR_MUTED));
  lv_label_set_long_mode(stats_label, LV_LABEL_LONG_CLIP);
  lv_obj_set_flex_grow(stats_label, 1);
  toolbar_btn = lv_btn_create(toolbar);
  lv_obj_set_size(toolbar_btn, px(36), px(36));
  lv_obj_set_style_pad_all(toolbar_btn, 2, 0);
  lv_obj_set_style_bg_color(toolbar_btn, lv_color_hex(COLOR_SECONDARY), 0);
  lv_obj_set_style_bg_opa(toolbar_btn, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(toolbar_btn, lv_color_hex(COLOR_PRESSED), LV_STATE_PRESSED);
  lv_obj_set_style_shadow_width(toolbar_btn, 0, 0);
  lv_obj_set_style_border_width(toolbar_btn, 0, 0);
  lv_obj_set_style_radius(toolbar_btn, 6, 0);
  lv_obj_add_event_cb(toolbar_btn, &FilesExtraView::_action_clicked, LV_EVENT_CLICKED, this);
  lv_obj_add_flag(toolbar, LV_OBJ_FLAG_HIDDEN);

  // List (same look as the file grid).
  list = lv_obj_create(left_parent);
  lv_obj_set_width(list, LV_PCT(100));
  lv_obj_set_height(list, LV_SIZE_CONTENT);
  lv_obj_set_flex_grow(list, 1);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_all(list, 0, 0);
  lv_obj_set_style_pad_row(list, px(4), 0);
  lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(list, 0, 0);
  lv_obj_set_style_radius(list, 0, 0);
  lv_obj_set_scroll_dir(list, LV_DIR_VER);
  lv_obj_add_flag(list, LV_OBJ_FLAG_HIDDEN);

  // Detail card.
  detail = lv_obj_create(detail_parent);
  lv_obj_set_size(detail, px(292), LV_PCT(100));
  lv_obj_clear_flag(detail, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(detail, lv_color_hex(COLOR_CARD), 0);
  lv_obj_set_style_bg_opa(detail, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(detail, 1, 0);
  lv_obj_set_style_border_color(detail, lv_color_hex(COLOR_WHITE), 0);
  lv_obj_set_style_border_opa(detail, LV_OPA_10, 0);
  lv_obj_set_style_radius(detail, px(14), 0);
  lv_obj_set_style_pad_all(detail, 0, 0);
  lv_obj_add_flag(detail, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_to_index(detail, 1);  // right after the file list card (only one of the two is visible)
}

FilesExtraView::~FilesExtraView() {
  // The widgets belong to PrintPanel's containers, which are deleted with it.
}

bool FilesExtraView::printing() const {
  auto st = State::get_instance()->get_data("/printer_state/print_stats/state"_json_pointer);
  const std::string state = st.is_string() ? st.template get<std::string>() : "";
  return state == "printing" || state == "paused";
}

void FilesExtraView::show(Mode m) {
  mode = m;
  shown = true;
  selected = -1;
  jobs.clear();
  videos.clear();
  totals_text.clear();
  lv_obj_clear_flag(toolbar, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(list, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(detail, LV_OBJ_FLAG_HIDDEN);
  rebuild_toolbar();
  rebuild_list();
  rebuild_detail();
  refresh();
}

void FilesExtraView::hide() {
  shown = false;
  if (!thumb_src.empty()) {
    lv_img_cache_invalidate_src(thumb_src.c_str());
    thumb_src.clear();
  }
  lv_obj_add_flag(toolbar, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(list, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(detail, LV_OBJ_FLAG_HIDDEN);
}

void FilesExtraView::refresh() {
  const Mode requested = mode;
  if (requested == Mode::History) {
    ws.send_jsonrpc("server.history.totals", json::object(), [this](json &j) {
      std::lock_guard<std::mutex> lock(lv_lock);
      auto totals = j.find("result");
      if (totals != j.end() && totals->contains("job_totals")) {
        auto &t = (*totals)["job_totals"];
        totals_text = fmt::format("{} prints  -  {}  -  {}", t.value("total_jobs", 0),
                                  format_duration(t.value("total_time", 0.0)), format_length(t.value("total_filament_used", 0.0)));
      }
      if (shown && mode == Mode::History) {
        rebuild_toolbar();
      }
    });
    ws.send_jsonrpc("server.history.list", json{{"limit", 30}, {"order", "desc"}}, [this](json &j) {
      std::lock_guard<std::mutex> lock(lv_lock);
      jobs.clear();
      auto result = j.find("result");
      if (result != j.end() && result->contains("jobs")) {
        for (auto &job : (*result)["jobs"]) {
          HistoryJob h;
          h.filename = job.value("filename", "");
          h.status = job.value("status", "");
          h.start = job.value("start_time", 0.0);
          h.duration = job.value("print_duration", 0.0);
          h.filament_mm = job.value("filament_used", 0.0);
          h.exists = job.value("exists", false);
          if (job.contains("metadata") && job["metadata"].is_object()) {
            h.weight_g = job["metadata"].value("filament_weight_total", 0.0);
          }
          jobs.push_back(h);
        }
      }
      if (shown && mode == Mode::History) {
        rebuild_list();
        rebuild_detail();
      }
    });
  } else {
    ws.send_jsonrpc("server.files.list", json{{"root", "timelapse"}}, [this](json &j) {
      std::lock_guard<std::mutex> lock(lv_lock);
      videos.clear();
      auto result = j.find("result");
      if (result != j.end() && result->is_array()) {
        std::set<std::string> files;
        for (auto &f : *result) {
          files.insert(f.value("path", ""));
        }
        for (auto &f : *result) {
          const std::string path = f.value("path", "");
          if (path.size() > 4 && path.compare(path.size() - 4, 4, ".mp4") == 0) {
            // moonraker-timelapse leaves a preview image with the same name next to each rendered video.
            const std::string preview = path.substr(0, path.size() - 4) + ".jpg";
            videos.push_back({path, f.value("modified", 0.0), f.value("size", 0.0), files.count(preview) ? preview : ""});
          }
        }
        std::sort(videos.begin(), videos.end(), [](const Video &a, const Video &b) { return a.modified > b.modified; });
      }
      if (shown && mode == Mode::Timelapse) {
        rebuild_toolbar();
        rebuild_list();
        rebuild_detail();
      }
    });
  }
}

void FilesExtraView::rebuild_toolbar() {
  lv_obj_clean(toolbar_btn);
  if (mode == Mode::History) {
    lv_label_set_text(stats_label, totals_text.c_str());
    lv_obj_set_style_text_color(stats_label, lv_color_hex(COLOR_FG), 0);
    lv_obj_center(icon(toolbar_btn, &ui_icon_trash, 18, lv_color_hex(COLOR_DESTRUCTIVE)));
  } else {
    double total = 0;
    for (auto &v : videos) {
      total += v.size;
    }
    lv_label_set_text(stats_label, fmt::format("{} videos  -  {}", videos.size(), format_size(total)).c_str());
    lv_obj_set_style_text_color(stats_label, lv_color_hex(COLOR_MUTED), 0);
    lv_obj_t *l = lv_label_create(toolbar_btn);
    lv_label_set_text(l, LV_SYMBOL_REFRESH);
    lv_obj_center(l);
  }
}

lv_obj_t *FilesExtraView::add_row(const char *title, const std::string &subtitle, bool video, lv_color_t dot, bool is_selected, int index) {
  lv_obj_t *row = plain(list);
  lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(row, LV_PCT(100), px(56));
  lv_obj_set_style_radius(row, px(10), 0);
  lv_obj_set_style_bg_color(row, lv_color_hex(COLOR_SECONDARY), 0);
  lv_obj_set_style_bg_opa(row, is_selected ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
  lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_STATE_PRESSED);
  lv_obj_set_style_bg_color(row, lv_color_hex(COLOR_PRESSED), LV_STATE_PRESSED);
  lv_obj_set_style_border_width(row, 1, 0);
  lv_obj_set_style_border_color(row, lv_color_hex(COLOR_ACCENT), 0);
  lv_obj_set_style_border_opa(row, is_selected ? LV_OPA_50 : LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_hor(row, px(10), 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(row, px(12), 0);

  lv_obj_t *tile = plain(row);
  lv_obj_set_size(tile, px(40), px(40));
  lv_obj_set_style_radius(tile, px(8), 0);
  lv_obj_set_style_bg_color(tile, lv_color_hex(COLOR_BG), 0);
  lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(tile, 1, 0);
  lv_obj_set_style_border_color(tile, lv_color_hex(COLOR_WHITE), 0);
  lv_obj_set_style_border_opa(tile, LV_OPA_10, 0);
  lv_obj_center(icon(tile, video ? &ui_icon_camera : &print, 22, lv_color_hex(COLOR_MUTED)));

  lv_obj_t *text = plain(row);
  lv_obj_set_height(text, LV_SIZE_CONTENT);
  lv_obj_set_flex_grow(text, 1);
  lv_obj_set_flex_flow(text, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(text, 2, 0);
  lv_obj_t *t = label(text, title, &lv_font_montserrat_14, lv_color_hex(COLOR_FG));
  lv_label_set_long_mode(t, LV_LABEL_LONG_DOT);
  lv_obj_set_width(t, LV_PCT(100));
  lv_obj_t *s = label(text, subtitle.c_str(), &lv_font_montserrat_12, lv_color_hex(COLOR_MUTED));
  lv_label_set_long_mode(s, LV_LABEL_LONG_DOT);
  lv_obj_set_width(s, LV_PCT(100));

  if (video) {
    label(row, LV_SYMBOL_RIGHT, &lv_font_montserrat_14, lv_color_hex(COLOR_MUTED));
  } else {
    lv_obj_t *d = plain(row);
    lv_obj_set_size(d, px(9), px(9));
    lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(d, dot, 0);
    lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
  }

  row_contexts.push_back(std::unique_ptr<RowContext>(new RowContext{this, index}));
  lv_obj_add_event_cb(row, &FilesExtraView::_row_clicked, LV_EVENT_CLICKED, row_contexts.back().get());
  return row;
}

void FilesExtraView::rebuild_list() {
  lv_obj_clean(list);
  row_contexts.clear();
  if (mode == Mode::History) {
    if (jobs.empty()) {
      empty_state(list, &ui_icon_folder, "No print history", "Finished prints appear here once Moonraker has recorded them.");
      return;
    }
    for (size_t i = 0; i < jobs.size(); i++) {
      const HistoryJob &h = jobs[i];
      lv_color_t dot = lv_color_hex(h.status == "completed" ? COLOR_ACCENT
                                    : h.status == "in_progress" ? COLOR_WARNING : COLOR_DESTRUCTIVE);
      add_row(strip_ext(base_name(h.filename)).c_str(),
              fmt::format("{}  -  {}", format_time(h.start), format_duration(h.duration)), false, dot, (int)i == selected, (int)i);
    }
  } else {
    if (videos.empty()) {
      empty_state(list, &ui_icon_camera, "No timelapse videos", "Videos recorded by the timelapse module appear here.");
      return;
    }
    for (size_t i = 0; i < videos.size(); i++) {
      const Video &v = videos[i];
      add_row(base_name(v.path).c_str(), fmt::format("{}  -  {}", format_time(v.modified), format_size(v.size)), true,
              lv_color_hex(COLOR_MUTED), (int)i == selected, (int)i);
    }
  }
}

void FilesExtraView::add_info_row(int y, const char *name, const std::string &value) {
  lv_obj_t *sep = plain(detail);
  lv_obj_set_size(sep, px(260), 1);
  lv_obj_set_pos(sep, px(16), px(y));
  lv_obj_set_style_bg_color(sep, lv_color_hex(COLOR_WHITE), 0);
  lv_obj_set_style_bg_opa(sep, LV_OPA_10, 0);
  lv_obj_t *n = label(detail, name, &lv_font_montserrat_14, lv_color_hex(COLOR_MUTED));
  lv_obj_set_pos(n, px(16), px(y + 8));
  lv_obj_t *v = label(detail, value.c_str(), &lv_font_montserrat_14, lv_color_hex(COLOR_FG));
  lv_obj_align(v, LV_ALIGN_TOP_RIGHT, -px(16), px(y + 8));
}

void FilesExtraView::rebuild_detail() {
  if (!thumb_src.empty()) {
    lv_img_cache_invalidate_src(thumb_src.c_str());  // a decoded 1280x720 preview is large: do not keep old ones
    thumb_src.clear();
  }
  lv_obj_clean(detail);
  primary_btn = NULL;
  delete_btn = NULL;

  const bool have = mode == Mode::History ? selected >= 0 && selected < (int)jobs.size()
                                          : selected >= 0 && selected < (int)videos.size();

  lv_obj_t *preview = plain(detail);
  lv_obj_set_pos(preview, px(16), px(16));
  lv_obj_set_size(preview, px(260), px(150));
  lv_obj_set_style_radius(preview, px(10), 0);
  lv_obj_set_style_bg_color(preview, lv_color_hex(COLOR_BG), 0);
  lv_obj_set_style_bg_opa(preview, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(preview, 1, 0);
  lv_obj_set_style_border_color(preview, lv_color_hex(COLOR_WHITE), 0);
  lv_obj_set_style_border_opa(preview, LV_OPA_10, 0);
  lv_obj_clear_flag(preview, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_clip_corner(preview, true, 0);
  std::string preview_file;
  if (mode == Mode::Timelapse && have && !videos[selected].thumb.empty()) {
    const std::string &thumb = videos[selected].thumb;
    preview_file = KUtils::is_running_local()
                       ? KUtils::get_root_path("timelapse") + "/" + thumb
                       : KUtils::download_file("timelapse", thumb, Config::get_instance()->get_thumbnail_path());
  }
  if (!preview_file.empty()) {
    thumb_src = "A:" + preview_file;
    lv_img_cache_invalidate_src(thumb_src.c_str());
    lv_obj_t *shot = lv_img_create(preview);
    lv_img_set_src(shot, thumb_src.c_str());
    lv_img_set_zoom(shot, 256 * px(260) / 1280);  // the camera frame is 1280x720: scale it to the 260 px box
    lv_obj_clear_flag(shot, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(shot);
  } else {
    lv_obj_center(icon(preview, mode == Mode::Timelapse ? &ui_icon_camera : &print, 56, lv_color_hex(COLOR_MUTED)));
  }

  if (!have) {
    lv_obj_t *hint = label(detail, mode == Mode::History ? "Select a print to see its details" : "Select a video to see its details",
                           &lv_font_montserrat_14, lv_color_hex(COLOR_MUTED));
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(hint, px(260));
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(hint, px(16), px(190));
    return;
  }

  if (mode == Mode::History) {
    const HistoryJob &h = jobs[selected];
    lv_obj_t *title = label(detail, strip_ext(base_name(h.filename)).c_str(), &lv_font_montserrat_16, lv_color_hex(COLOR_FG));
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
    lv_obj_set_width(title, px(260));
    lv_obj_set_pos(title, px(16), px(176));
    const bool ok = h.status == "completed";
    std::string status = h.status;
    if (!status.empty()) {
      status[0] = static_cast<char>(std::toupper(status[0]));
    }
    lv_obj_t *b = badge(detail, status.c_str(), lv_color_hex(ok ? COLOR_ACCENT : h.status == "in_progress" ? COLOR_WARNING : COLOR_DESTRUCTIVE));
    lv_obj_set_pos(b, px(16), px(202));
    add_info_row(236, "Started", format_time(h.start));
    add_info_row(270, "Duration", format_duration(h.duration));
    add_info_row(304, "Filament", h.weight_g > 0 ? fmt::format("{:.0f} g", h.weight_g) : fmt::format("{:.1f} m", h.filament_mm / 1000.0));
    primary_btn = action_button(detail, &print, "Print again", ActionKind::Primary, 16, 348, 260, 52, &FilesExtraView::_action_clicked, this);
    if (!h.exists || printing()) {
      lv_obj_add_state(primary_btn, LV_STATE_DISABLED);
    }
  } else {
    const Video &v = videos[selected];
    lv_obj_t *title = label(detail, base_name(v.path).c_str(), &lv_font_montserrat_16, lv_color_hex(COLOR_FG));
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
    lv_obj_set_width(title, px(260));
    lv_obj_set_pos(title, px(16), px(176));
    lv_obj_t *sub = label(detail, fmt::format("Created {}", format_time(v.modified)).c_str(), &lv_font_montserrat_12, lv_color_hex(COLOR_MUTED));
    lv_obj_set_pos(sub, px(16), px(200));
    add_info_row(224, "Size", format_size(v.size));
    lv_obj_t *note = label(detail, "The screen cannot play videos. Open the file from Fluidd or copy it with USB.",
                           &lv_font_montserrat_12, lv_color_hex(COLOR_MUTED));
    lv_label_set_long_mode(note, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(note, px(260));
    lv_obj_set_pos(note, px(16), px(268));
    primary_btn = action_button(detail, &sd_img, "Copy to USB", ActionKind::Outline, 16, 348, 196, 52, &FilesExtraView::_action_clicked, this);
    delete_btn = action_button(detail, &ui_icon_trash, "", ActionKind::Destructive, 224, 348, 52, 52, &FilesExtraView::_action_clicked, this);
    lv_obj_set_style_pad_column(delete_btn, 0, 0);
  }
}

void FilesExtraView::notify(const std::string &title, const std::string &message) {
  confirm_dialog(title.c_str(), message.c_str(), "OK", ActionKind::Outline, []() {});
}

void FilesExtraView::handle_row(int index) {
  selected = index;
  rebuild_list();
  rebuild_detail();
}

void FilesExtraView::_row_clicked(lv_event_t *event) {
  RowContext *ctx = (RowContext *)event->user_data;
  // rebuild_list() deletes the clicked row while its event runs: defer the work.
  FilesExtraView *view = ctx->view;
  const int index = ctx->index;
  lv_async_call([](void *data) {
    auto *pair = (std::pair<FilesExtraView *, int> *)data;
    pair->first->handle_row(pair->second);
    delete pair;
  }, new std::pair<FilesExtraView *, int>(view, index));
}

void FilesExtraView::_action_clicked(lv_event_t *event) {
  ((FilesExtraView *)event->user_data)->handle_action(event);
}

void FilesExtraView::handle_action(lv_event_t *event) {
  lv_obj_t *btn = lv_event_get_current_target(event);

  if (btn == toolbar_btn) {
    if (mode == Mode::History) {
      confirm_dialog("Reset totals?", "The counters go back to zero. The list of prints is not deleted.", "Reset",
                     ActionKind::Destructive, [this]() {
        ws.send_jsonrpc("server.history.reset_totals", json::object(), [this](json &) { refresh(); });
      });
    } else {
      refresh();
    }
    return;
  }

  if (mode == Mode::History && btn == primary_btn && selected >= 0 && selected < (int)jobs.size()) {
    const std::string file = jobs[selected].filename;
    if (!printing() && start_print) {
      start_print(file);
    }
    return;
  }

  if (mode == Mode::Timelapse && selected >= 0 && selected < (int)videos.size()) {
    const Video video = videos[selected];
    if (btn == primary_btn) {
      json params = {{"source", "timelapse/" + video.path}, {"dest", "gcodes/usb/" + base_name(video.path)}};
      ws.send_jsonrpc("server.files.copy", params, [this](json &j) {
        std::lock_guard<std::mutex> lock(lv_lock);
        if (j.contains("error")) {
          spdlog::warn("copy timelapse to USB failed: {}", j.dump());
          notify("Could not copy", "Insert a USB drive and try again.");
        } else {
          notify("Copied to USB", "The video is on the USB drive.");
        }
      });
    } else if (btn == delete_btn) {
      confirm_dialog("Delete video?", fmt::format("{} will be deleted permanently.", base_name(video.path)).c_str(), "Delete",
                     ActionKind::Destructive, [this, video]() {
        ws.send_jsonrpc("server.files.delete_file", json{{"path", "timelapse/" + video.path}}, [this](json &) {
          std::lock_guard<std::mutex> lock(lv_lock);
          selected = -1;
          refresh();
        });
      });
    }
  }
}
