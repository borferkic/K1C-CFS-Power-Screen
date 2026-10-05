#include "cfs_model.h"
#include "spdlog/spdlog.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <map>

namespace cfs {

namespace {

using json = nlohmann::json;

struct MaterialEntry {
  std::string brand;
  std::string name;
  std::string type;
  std::string color;
};

std::vector<Material> g_materials;

// Creality material database (id -> brand, name, type), read once from the printer. The path can be overridden
// with POWERSCREEN_CFS_MATERIAL_DB (used by the emulator).
const std::map<std::string, MaterialEntry> &material_db() {
  static std::map<std::string, MaterialEntry> db;
  static bool loaded = false;
  if (loaded) {
    return db;
  }
  loaded = true;

  const char *env = std::getenv("POWERSCREEN_CFS_MATERIAL_DB");
  const std::string path = env != nullptr && *env != '\0'
                             ? std::string(env)
                             : std::string("/usr/data/creality/userdata/box/material_database.json");
  std::ifstream in(path);
  if (!in.good()) {
    spdlog::debug("cfs: no material database at {}", path);
    return db;
  }
  json j = json::parse(in, nullptr, false);
  if (j.is_discarded() || !j.is_object() || !j.contains("result")) {
    spdlog::debug("cfs: material database {} is not readable", path);
    return db;
  }
  const json &list = j["result"].is_object() && j["result"].contains("list") ? j["result"]["list"] : j["result"];
  if (!list.is_array()) {
    return db;
  }
  for (const auto &entry : list) {
    if (!entry.is_object() || !entry.contains("base") || !entry["base"].is_object()) {
      continue;
    }
    const json &base = entry["base"];
    MaterialEntry m;
    m.brand = base.value("brand", std::string());
    m.name = base.value("name", std::string());
    m.type = base.value("meterialType", std::string());  // sic: Creality's key
    if (entry.contains("kvParam") && entry["kvParam"].is_object()) {
      const json &color = entry["kvParam"].value("default_filament_colour", json());
      if (color.is_string()) {
        m.color = color.get<std::string>();
      }
    }
    const std::string id = base.value("id", std::string());
    if (!id.empty()) {
      db[id] = m;
      Material item;
      item.id = id;
      item.brand = m.brand;
      item.name = m.name;
      item.type = m.type;
      item.has_color = false;
      g_materials.push_back(item);
    }
  }
  spdlog::debug("cfs: {} materials loaded from {}", db.size(), path);
  return db;
}

std::string str_at(const json &arr, int i) {
  if (arr.is_array() && i >= 0 && static_cast<size_t>(i) < arr.size() && arr[i].is_string()) {
    return arr[i].get<std::string>();
  }
  return std::string();
}

bool is_none(const std::string &s) { return s.empty() || s == "-1" || s == "none" || s == "None"; }

bool is_hex(const std::string &s) {
  if (s.empty()) {
    return false;
  }
  for (char c : s) {
    if (!std::isxdigit(static_cast<unsigned char>(c))) {
      return false;
    }
  }
  return true;
}

// Accepts "#RRGGBB", "RRGGBB" and "0RRGGBB" (Creality's 7-character form).
bool parse_color(const std::string &raw, uint32_t &out) {
  std::string s = raw;
  if (!s.empty() && s[0] == '#') {
    s = s.substr(1);
  } else if (s.size() == 7 && s[0] == '0') {
    s = s.substr(1);
  }
  if (s.size() != 6 || !is_hex(s)) {
    return false;
  }
  out = static_cast<uint32_t>(std::strtoul(s.c_str(), nullptr, 16));
  return true;
}

int parse_len(const std::string &s) {
  if (s.empty() || s[0] == '-') {
    return -1;
  }
  for (char c : s) {
    if (!std::isdigit(static_cast<unsigned char>(c))) {
      return -1;
    }
  }
  return std::atoi(s.c_str());
}

Slot parse_slot(const json &box_obj, int i) {
  Slot slot;
  const std::string vender = str_at(box_obj.value("vender", json()), i);
  const std::string mat = str_at(box_obj.value("material_type", json()), i);
  const std::string col = str_at(box_obj.value("color_value", json()), i);
  slot.remain_len = parse_len(str_at(box_obj.value("remain_len", json()), i));
  slot.has_color = parse_color(col, slot.color);

  // A real material id (not "unknown"/"-1") makes the slot Defined; a color alone, or any "unknown" field, means a
  // spool is loaded but nothing is known about its material (the usual case: no RFID tag).
  const bool has_material = !is_none(mat) && mat != "unknown";
  const bool has_vender = !is_none(vender) && vender != "unknown";
  const bool any_unknown = vender == "unknown" || mat == "unknown" || col == "unknown";
  if (has_material) {
    slot.kind = SlotKind::Defined;
    slot.material_id = mat;
    const auto &db = material_db();
    auto it = db.find(mat);
    if (it != db.end()) {
      slot.name = it->second.name;
      slot.material = it->second.type;
      slot.brand = it->second.brand;
      if (!slot.has_color) {
        slot.has_color = parse_color(it->second.color, slot.color);
      }
    } else {
      slot.name = "Material " + mat;
    }
    if (has_vender) {
      slot.brand = vender;
    }
  } else if (slot.has_color || any_unknown || has_vender) {
    slot.kind = SlotKind::Unset;
    slot.name = "Not set";
    if (has_vender) {
      slot.brand = vender;
    }
  } else {
    slot.kind = SlotKind::Empty;
    slot.name = "Empty";
  }
  return slot;
}

}  // namespace

const std::vector<Material> &materials() {
  const auto &db = material_db();  // loads the database once
  static bool colors_filled = false;
  if (!colors_filled) {
    colors_filled = true;
    for (auto &item : g_materials) {
      auto it = db.find(item.id);
      if (it != db.end()) {
        item.has_color = parse_color(it->second.color, item.color);
      }
    }
  }
  return g_materials;
}

bool find_material(const std::string &id, Material &out) {
  for (const auto &m : materials()) {
    if (m.id == id) {
      out = m;
      return true;
    }
  }
  return false;
}

State parse(const json &box) {
  State state;
  if (!box.is_object()) {
    return state;
  }
  state.connected = box.value("state", std::string()) == "connect";
  if (!state.connected) {
    return state;
  }

  // First box that reports itself as connected (T1..T4); T1 when none does.
  int found = 0;
  for (int n = 1; n <= 4 && found == 0; ++n) {
    const std::string key = "T" + std::to_string(n);
    if (box.contains(key) && box[key].is_object() && box[key].value("state", std::string()) == "connect") {
      found = n;
    }
  }
  state.box = found > 0 ? found : 1;
  const std::string key = "T" + std::to_string(state.box);
  if (box.contains(key) && box[key].is_object()) {
    const json &b = box[key];
    state.version = b.value("version", std::string());
    if (state.version == "-1") {
      state.version.clear();
    }
    for (int i = 0; i < 4; ++i) {
      state.slots[i] = parse_slot(b, i);
    }
  }

  // Last T command, "T1A" form: marks the active slot of this box.
  const std::string t_command = box.value("t_command", std::string());
  if (t_command.size() == 3 && t_command[0] == 'T' && t_command[1] - '0' == state.box) {
    const int letter = std::toupper(static_cast<unsigned char>(t_command[2])) - 'A';
    if (letter >= 0 && letter < 4) {
      state.active = letter;
    }
  }
  return state;
}

bool same(const State &a, const State &b) {
  if (a.connected != b.connected || a.box != b.box || a.version != b.version || a.active != b.active) {
    return false;
  }
  for (int i = 0; i < 4; ++i) {
    const Slot &x = a.slots[i];
    const Slot &y = b.slots[i];
    if (x.kind != y.kind || x.name != y.name || x.material != y.material || x.brand != y.brand
        || x.has_color != y.has_color || x.color != y.color || x.remain_len != y.remain_len
        || x.material_id != y.material_id) {
      return false;
    }
  }
  return true;
}

}  // namespace cfs
