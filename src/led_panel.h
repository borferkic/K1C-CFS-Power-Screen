#ifndef __LED_PANEL_H__
#define __LED_PANEL_H__

#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <utility>

#include "websocket_client.h"
#include "notify_consumer.h"

class LedPanel : public NotifyConsumer {
 public:
  LedPanel(KWebSocketClient &, std::mutex &);
  ~LedPanel();

  void consume(json &j);
  void init(json&);
  void refresh();
  void toggle();
  void set_state_callback(std::function<void(bool)> callback);

 private:
  void notify_state();
  void send_value(const std::string &id, double value);

  KWebSocketClient &ws;
  std::map<std::string, bool> leds;
  std::function<void(bool)> state_callback;

};

#endif // __LED_PANEL_H__
