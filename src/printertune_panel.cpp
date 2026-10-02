#include "printertune_panel.h"

LV_IMG_DECLARE(ui_console_img);
#include "state.h"
#include "powerui.h"
#include "utils.h"
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

LV_IMG_DECLARE(ui_icon_plug_zap);
LV_IMG_DECLARE(cooldown_img);
LV_IMG_DECLARE(extruder);

// Row heights of the tile grid: two rows fill the panel; more rows keep that height and the panel scrolls.
static lv_coord_t grid_col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
static lv_coord_t grid_rows[6] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};

PrinterTunePanel::PrinterTunePanel(KWebSocketClient &c, std::mutex &l, lv_obj_t *parent, FineTunePanel &finetune)
  : ws(c)
  , cont(lv_obj_create(parent))
  , lv_lock(l)
  , tmc_tune_available(false)
  , tmc_status_available(false)
  , power_devices_available(false)
  , nozzle_clean_mode(0)
  , bedmesh_panel(c, l)
  , finetune_panel(finetune)
  , limits_panel(c, l)
  , inputshaper_panel(c, l)
  , belts_calibration_panel(c, l)
  , tmc_tune_panel(c)
  , tmc_status_panel(c, l)
  , power_panel(c, l)
  , pid_tune_panel(c, l)
  , bedmesh_btn(cont, &bedmesh_img, "Bed Mesh", &PrinterTunePanel::_handle_callback, this)
  , finetune_btn(cont, &fine_tune_img, "Fine Tune", &PrinterTunePanel::_handle_callback, this)
  , inputshaper_btn(cont, &inputshaper_img, "Input Shaper", &PrinterTunePanel::_handle_callback, this)
  , belts_calibration_btn(cont, &inputshaper_img, "Belts / Shake", &PrinterTunePanel::_handle_callback, this)
  , limits_btn(cont, &limit_img, "Limits", &PrinterTunePanel::_handle_callback, this)
  , tmc_tune_btn(cont, &motor_img, "TMC Autotune", &PrinterTunePanel::_handle_callback, this)
  , tmc_status_btn(cont, &chart_img, "TMC Metrics", &PrinterTunePanel::_handle_callback, this)
  , power_devices_btn(cont, &ui_icon_plug_zap, "Power Devices", &PrinterTunePanel::_handle_callback, this)
  , console_btn(cont, &ui_console_img, "Console", &PrinterTunePanel::_handle_callback, this)
  , pid_tune_btn(cont, &cooldown_img, "PID Tune", &PrinterTunePanel::_handle_callback, this)
  , nozzle_clean_btn(cont, &extruder, "Nozzle Clean", &PrinterTunePanel::_handle_callback, this)
{
  bedmesh_btn.set_subtitle("Probe the bed surface");
  finetune_btn.set_subtitle("Z, speed, flow and PA");
  inputshaper_btn.set_subtitle("Resonance and shapers");
  belts_calibration_btn.set_subtitle("Belt tension and shake");
  limits_btn.set_subtitle("Velocity and acceleration");
  tmc_tune_btn.set_subtitle("Motor driver tuning");
  tmc_status_btn.set_subtitle("Stepper driver statistics");
  power_devices_btn.set_subtitle("Printer power outlets");
  console_btn.set_subtitle("G-code and macros");
  pid_tune_btn.set_subtitle("Hotend and bed");
  nozzle_clean_btn.set_subtitle("Clean the nozzle");

  lv_obj_move_background(cont);

  lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));

  tmc_tune_btn.disable();

  lv_obj_set_grid_dsc_array(cont, grid_col_dsc, grid_rows);
  lv_obj_set_style_pad_all(cont, 12, LV_PART_MAIN);
  lv_obj_set_style_pad_row(cont, 12, LV_PART_MAIN);
  lv_obj_set_style_pad_column(cont, 12, LV_PART_MAIN);

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
    {&console_btn, true},
    {&pid_tune_btn, true},
    {&nozzle_clean_btn, nozzle_clean_mode != 0},
  };

  int slot = 0;
  for (const auto &entry : buttons) {
    lv_obj_t *btn = entry.first->get_button();
    if (!entry.second) {
      lv_obj_add_flag(btn, LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_STRETCH, slot % 4, 1,
                         LV_GRID_ALIGN_STRETCH, slot / 4, 1);
    slot++;
  }
  // Rows: two fill the panel; from the third row on each row keeps the height of a two-row layout and the tab scrolls.
  const int rows = std::max(2, (slot + 3) / 4);
  const lv_coord_t row_height = (powerui::overlay_height_px() - 3 * 12) / 2;
  for (int r = 0; r < rows; r++) {
    grid_rows[r] = rows > 2 ? row_height : LV_GRID_FR(1);
  }
  grid_rows[rows] = LV_GRID_TEMPLATE_LAST;
  lv_obj_set_grid_dsc_array(cont, grid_col_dsc, grid_rows);
  if (rows > 2) {
    lv_obj_add_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(cont, LV_DIR_VER);
  } else {
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
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
  pid_tune_panel.init(j);

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
  nozzle_clean_mode = 0;
  auto &objects = s->get_data("/printer_objs/objects"_json_pointer);
  if (objects.is_array()) {
    for (auto &o : objects) {
      if (!o.is_string()) {
        continue;
      }
      std::string name = o.template get<std::string>();
      std::transform(name.begin(), name.end(), name.begin(), ::tolower);
      if (name == "gcode_macro _ps_load_module" || name == "tmcstatus") {
        tmc_status_available = true;
      }
      if (name == "box") {
        nozzle_clean_mode = 1;
      } else if (name == "gcode_macro ps_nocfs_clean_brush" && nozzle_clean_mode == 0) {
        nozzle_clean_mode = 2;
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
    } else if (btn == console_btn.get_button()) {
      spdlog::trace("console pressed");
      if (console_callback) {
        console_callback();
      }
    } else if (btn == power_devices_btn.get_button()) {
      spdlog::trace("power devices pressed");
      power_panel.foreground();
    } else if (btn == pid_tune_btn.get_button()) {
      spdlog::trace("pid tune pressed");
      pid_tune_panel.foreground();
    } else if (btn == nozzle_clean_btn.get_button()) {
      spdlog::trace("nozzle clean pressed");
      clean_nozzle();
    }
  }
}

// Wipe the nozzle on the brush: the CFS box command when the printer has it, else the Power Script brush macro.
void PrinterTunePanel::clean_nozzle() {
  State *st = State::get_instance();
  auto state = st->get_data("/printer_state/print_stats/state"_json_pointer);
  const std::string print_state = state.is_string() ? state.template get<std::string>() : "";
  if (print_state == "printing" || print_state == "paused") {
    powerui::confirm_dialog("Printing in progress", "Nozzle Clean is not available while printing.", "OK",
                            powerui::ActionKind::Outline, []() {});
    return;
  }

  const int mode = nozzle_clean_mode;
  powerui::confirm_dialog("Clean the nozzle?",
                          "The toolhead moves to the brush and wipes the nozzle. Keep the area around the brush clear.",
                          "Clean", powerui::ActionKind::Primary, [this, mode]() {
    std::string script;
    if (!KUtils::is_homed()) {
      script += "G28\n";
    }
    if (mode == 1) {
      script += "BOX_NOZZLE_CLEAN";
    } else {
      auto z = State::get_instance()->get_data("/printer_state/toolhead/position/2"_json_pointer);
      if (z.is_number() && z.template get<double>() < 30.0) {
        script += "G90\nG1 Z30 F600\n";
      }
      script += "G90\nG0 X148.5 F5000\nG0 Y225 F5000\nPS_NOCFS_CLEAN_BRUSH";
    }
    spdlog::debug("nozzle clean: {}", script);
    ws.gcode_script(script);
  });
}
