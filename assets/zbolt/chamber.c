/* Reutiliza el icono Material Design de GuppyScreen para la camara cerrada. */
#define LV_ATTRIBUTE_IMG_CHAMBER
#define heater_map chamber_map
#define heater chamber
#define LV_ATTRIBUTE_IMG_HEATER LV_ATTRIBUTE_IMG_CHAMBER
#include "../assets material/material_46/heater.c"
#undef LV_ATTRIBUTE_IMG_HEATER
#undef heater
#undef heater_map
