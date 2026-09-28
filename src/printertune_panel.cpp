#include "printertune_panel.h"
#include "state.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cctype>
#include <experimental/filesystem>
#include <utility>
#include <vector>

namespace fs = std::experimental::filesystem;

LV_IMG_DECLARE(bedmesh_img);
LV_IMG_DECLARE(fine_tune_img);
LV_IMG_DECLARE(inputshaper_img);
LV_IMG_DECLARE(limit_img);
LV_IMG_DECLARE(motor_img);
LV_IMG_DECLARE(chart_img);

LV_IMG_DECLARE(print);

PrinterTunePanel::PrinterTunePanel(KWebSocketClient &c, std::mutex &l, lv_obj_t *parent, FineTunePanel &finetune)
  : cont(lv_obj_create(parent))
  , lv_lock(l)
  , tmc_tune_available(false)
  , tmc_status_available(false)
  , power_devices_available(false)
  , bedmesh_panel(c, l)
  , finetune_panel(finetune)
  , limits_panel(c, l)
  , inputshaper_panel(c, l)
  , belts_calibration_panel(c, l)
  , tmc_tune_panel(c)
  , tmc_status_panel(c, l)
  , power_panel(c, l)
  , bedmesh_btn(cont, &bedmesh_img, "Bed Mesh", &PrinterTunePanel::_handle_callback, this)
  , finetune_btn(cont, &fine_tune_img, "Fine Tune", &PrinterTunePanel::_handle_callback, this)
  , inputshaper_btn(cont, &inputshaper_img, "Input Shaper", &PrinterTunePanel::_handle_callback, this)
  , belts_calibration_btn(cont, &inputshaper_img, "Belts/Shake", &PrinterTunePanel::_handle_callback, this)
  , limits_btn(cont, &limit_img, "Limits", &PrinterTunePanel::_handle_callback, this)
  , tmc_tune_btn(cont, &motor_img, "TMC Autotune", &PrinterTunePanel::_handle_callback, this)
  , tmc_status_btn(cont, &chart_img, "TMC Metrics", &PrinterTunePanel::_handle_callback, this)
  , power_devices_btn(cont, &print, "Power Devices", &PrinterTunePanel::_handle_callback, this)
{
  lv_obj_move_background(cont);

  lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));

  tmc_tune_btn.disable();

  static lv_coord_t grid_main_row_dsc[] = {20, 150, 150, LV_GRID_TEMPLATE_LAST};
  static lv_coord_t grid_main_col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
      LV_GRID_TEMPLATE_LAST};

  lv_obj_set_grid_dsc_array(cont, grid_main_col_dsc, grid_main_row_dsc);
  lv_obj_set_style_pad_row(cont, 20, LV_PART_MAIN);

  relayout();
}

void PrinterTunePanel::relayout() {
  const std::vector<std::pair<SquareButton *, bool>> buttons = {
    {&bedmesh_btn, true},
    {&finetune_btn, true},
    {&inputshaper_btn, true},
    {&belts_calibration_btn, true},
    {&limits_btn, true},
    {&tmc_tune_btn, tmc_tune_available},
    {&tmc_status_btn, tmc_status_available},
    {&power_devices_btn, power_devices_available},
  };

  int slot = 0;
  for (const auto &entry : buttons) {
    lv_obj_t *btn = entry.first->get_button();
    if (!entry.second) {
      lv_obj_add_flag(btn, LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_CENTER, slot % 4, 1,
                         LV_GRID_ALIGN_START, 1 + slot / 4, 1);
    slot++;
  }
  spdlog::debug("calibrations: {} buttons (tmc_tune={}, tmc_status={}, power={})",
                slot, tmc_tune_available, tmc_status_available, power_devices_available);
}

void PrinterTunePanel::set_power_devices(json &j) {
  power_panel.create_devices(j);

  auto &devices = j["/result/devices"_json_pointer];
  const bool available = devices.is_array() && !devices.empty();
  std::lock_guard<std::mutex> lock(lv_lock);
  if (available != power_devices_available) {
    power_devices_available = available;
    relayout();
  }
}

PrinterTunePanel::~PrinterTunePanel() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

lv_obj_t *PrinterTunePanel::get_container() {
  return cont;
}

BedMeshPanel& PrinterTunePanel::get_bedmesh_panel() {
  return bedmesh_panel;
}

PowerPanel& PrinterTunePanel::get_power_panel() {
  return power_panel;
}

void PrinterTunePanel::init(json &j) {
  limits_panel.init(j);

  tmc_status_panel.init(j);

  // TODO: handle remote powerscreen instance
  State *s = State::get_instance();
  tmc_tune_available = false;
  auto kp = s->get_data("/printer_info/klipper_path"_json_pointer);
  if (!kp.is_null()) {
    auto p = fs::path(kp.template get<std::string>()) / "klippy/extras/motor_database.cfg";
    if (fs::exists(p)) {
      tmc_tune_available = true;
      tmc_tune_btn.enable();
      tmc_tune_panel.init(j, p);
    }
  }

  // TMC Metrics needs the command that loads the tmcstatus module, or the
  // module to be loaded already.
  tmc_status_available = false;
  auto &objects = s->get_data("/printer_objs/objects"_json_pointer);
  if (objects.is_array()) {
    for (auto &o : objects) {
      if (!o.is_string()) {
        continue;
      }
      std::string name = o.template get<std::string>();
      std::transform(name.begin(), name.end(), name.begin(), ::tolower);
      if (name == "gcode_macro _powerscreen_load_module" || name == "tmcstatus") {
        tmc_status_available = true;
        break;
      }
    }
  }

  relayout();
}

void PrinterTunePanel::handle_callback(lv_event_t *event) {
  if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
    lv_obj_t *btn = lv_event_get_current_target(event);

    if (btn == finetune_btn.get_button()) {
      spdlog::trace("tune finetune pressed");
      finetune_panel.foreground();
    } else if (btn == bedmesh_btn.get_button()) {
      spdlog::trace("tune bedmesh pressed");
      bedmesh_panel.foreground();
    } else if (btn == inputshaper_btn.get_button()) {
      spdlog::trace("tune inputshaper pressed");
      inputshaper_panel.foreground();
    } else if (btn == belts_calibration_btn.get_button()) {
      spdlog::trace("tune belts pressed");
      belts_calibration_panel.foreground();
    } else if (btn == limits_btn.get_button()) {
      spdlog::trace("limits pressed");
      limits_panel.foreground();
    } else if (btn == tmc_tune_btn.get_button()) {
      spdlog::trace("tmc auto tune pressed");
      tmc_tune_panel.foreground();
    } else if (btn == tmc_status_btn.get_button()) {
      spdlog::trace("tmc metrics pressed");
      tmc_status_panel.foreground();
    } else if (btn == power_devices_btn.get_button()) {
      spdlog::trace("power devices pressed");
      power_panel.foreground();
    }
  }
}
