#ifndef __CFS_MODEL_H__
#define __CFS_MODEL_H__

#include "hv/json.hpp"

#include <cstdint>
#include <string>
#include <vector>

// Read-only model of the Klipper "box" object (the CFS): connection state and the four slots of the box.
namespace cfs {

// Empty: no spool. Unset: a spool is loaded but nothing is known about it (most spools: no RFID tag).
// Defined: material and/or color known (read from an RFID tag or set by hand).
enum class SlotKind { Empty, Unset, Defined };

struct Slot {
  SlotKind kind = SlotKind::Empty;
  std::string name;      // "Hyper PLA" or "Not set"
  std::string material;  // "PLA"
  std::string material_id;  // id in the Creality material database ("01001"), empty when unknown
  std::string brand;     // "Creality"
  bool has_color = false;
  uint32_t color = 0;    // 0xRRGGBB
  int remain_len = -1;   // meters, -1 when unknown
};

struct State {
  bool connected = false;
  int box = 1;             // 1..4 (T1..T4): the first connected box
  std::string version;
  Slot slots[4];
  int active = -1;         // slot index 0..3 of the last T command, -1 when unknown
};

// One entry of the Creality material database (what the CFS stores in `material_type`).
struct Material {
  std::string id;     // "01001"
  std::string brand;  // "Creality"
  std::string name;   // "Hyper PLA"
  std::string type;   // "PLA"
  bool has_color = false;
  uint32_t color = 0;  // default color, 0xRRGGBB
};

// All materials of the database in file order (empty when the database is not on the printer).
const std::vector<Material> &materials();
bool find_material(const std::string &id, Material &out);

// Builds the state from the (merged) "box" object of Moonraker's printer objects.
State parse(const nlohmann::json &box);

// True when every field that a screen shows is equal.
bool same(const State &a, const State &b);

}  // namespace cfs

#endif  // __CFS_MODEL_H__
