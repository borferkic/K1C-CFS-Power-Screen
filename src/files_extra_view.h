#ifndef __FILES_EXTRA_VIEW_H__
#define __FILES_EXTRA_VIEW_H__

#include "lvgl/lvgl.h"
#include "websocket_client.h"
#include "hv/json.hpp"

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

using json = nlohmann::json;

// The Timelapse and History views of the Files tab. Both reuse the layout of the file picker: a list in the left
// card (below the storage switch) and a detail card on the right. PrintPanel shows one of them instead of the
// file list when the matching segment is selected.
class FilesExtraView {
 public:
  enum class Mode { Timelapse, History };

  FilesExtraView(KWebSocketClient &ws, std::mutex &lock, lv_obj_t *left_parent, lv_obj_t *detail_parent,
                 std::function<void(const std::string &)> start_print);
  ~FilesExtraView();

  void show(Mode mode);
  void hide();
  bool visible() const { return shown; }

  void handle_row(int index);
  void handle_action(lv_event_t *event);

  static void _row_clicked(lv_event_t *event);
  static void _action_clicked(lv_event_t *event);

 private:
  struct HistoryJob {
    std::string filename;
    std::string status;
    double start = 0;
    double duration = 0;
    double filament_mm = 0;
    double weight_g = 0;
    bool exists = false;
  };
  struct Video {
    std::string path;
    double modified = 0;
    double size = 0;
  };
  struct RowContext {
    FilesExtraView *view;
    int index;
  };

  void refresh();
  void rebuild_list();
  void rebuild_toolbar();
  void rebuild_detail();
  lv_obj_t *add_row(const char *title, const std::string &subtitle, bool video, lv_color_t dot, bool selected, int index);
  void add_info_row(int y, const char *name, const std::string &value);
  void notify(const std::string &title, const std::string &message);
  bool printing() const;

  KWebSocketClient &ws;
  std::mutex &lv_lock;
  std::function<void(const std::string &)> start_print;

  Mode mode;
  bool shown;
  int selected;

  lv_obj_t *toolbar;
  lv_obj_t *stats_label;
  lv_obj_t *toolbar_btn;
  lv_obj_t *list;
  lv_obj_t *detail;
  lv_obj_t *primary_btn;
  lv_obj_t *delete_btn;

  std::vector<HistoryJob> jobs;
  std::vector<Video> videos;
  std::string totals_text;
  std::vector<std::unique_ptr<RowContext>> row_contexts;
};

#endif // __FILES_EXTRA_VIEW_H__
