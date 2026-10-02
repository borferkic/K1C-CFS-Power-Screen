#ifndef __POWER_PANEL_H__
#define __POWER_PANEL_H__

#include "button_container.h"
#include "lvgl/lvgl.h"
#include "websocket_client.h"

#include <string>
#include <mutex>

class PowerPanel {
  public:
    PowerPanel(KWebSocketClient &ws, std::mutex &l);
    ~PowerPanel();

    void create_devices(json &j);
    
    void foreground();
    void handle_callback(lv_event_t *event);

    static void _handle_callback(lv_event_t *event) {
      PowerPanel *panel = (PowerPanel*)event->user_data;
      panel->handle_callback(event);
    };

  private:
    void create_device(json &j);
    void handle_device_callback(json &j);

    KWebSocketClient &ws;
    std::mutex &lv_lock;

    lv_obj_t *cont;
    ButtonContainer back_btn;

    struct Device {
      lv_obj_t *toggle;
      lv_obj_t *state;  // On / Off badge
    };
    void set_state(Device &d, bool on);

    lv_obj_t *list_card;   // card with a row per device
    lv_obj_t *empty;       // shown while there are no devices
    std::map<std::string, Device> devices;
  
};

#endif //__POWER_PANEL_H__
