#ifndef __CFS_MODEL_H__
#define __CFS_MODEL_H__

#include "hv/json.hpp"

#include <cstdint>
#include <string>

// Read-only model of the Klipper "box" object (the CFS): connection state and the four slots of the box.
namespace cfs {

// Empty: no spool. Unset: a spool is loaded but nothing is known about it (most spools: no RFID tag).
// Defined: material and/or color known (read from an RFID tag or set by hand).
enum class SlotKind { Empty, Unset, Defined };

struct Slot {
  SlotKind kind = SlotKind::Empty;
  std::string name;      // "Hyper PLA" or "Not set"
  std::string material;  // "PLA"
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

// Builds the state from the (merged) "box" object of Moonraker's printer objects.
State parse(const nlohmann::json &box);

// True when every field that a screen shows is equal.
bool same(const State &a, const State &b);

}  // namespace cfs

#endif  // __CFS_MODEL_H__
