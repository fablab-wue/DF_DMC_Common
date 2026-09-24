#pragma once

// Dragonframe GIO: open-collector outputs, pull-up inputs, camera, buzzer.
// Pin 255 means that function is absent. A move pin, when present, is driven high by the firmware.

#include <Arduino.h>
#include <cstdint>

namespace dfdmc {

struct GioMap {
  const uint8_t* outPins = nullptr;
  uint8_t outCount = 0;
  const uint8_t* inPins = nullptr;
  uint8_t inCount = 0;
  uint8_t cameraPin = 255;
  uint8_t buzzerPin = 255;
  uint8_t movePin = 255;
};

class DmcGio {
 public:
  void begin(const GioMap& map);
  void setOutputs(uint32_t bits);
  uint32_t outputs() const { return outBits_; }
  uint32_t inputs() const { return inStable_; }
  bool pollInputChange();
  void setCameraShutter(bool on);
  bool cameraShutter() const { return camShutter_; }
  void pulseBuzzer(unsigned ms);
  void tick();

 private:
  uint32_t readInputs() const;
  static void ocWrite(uint8_t pin, bool active);

  GioMap map_{};
  uint32_t outBits_ = 0;
  uint32_t inBits_ = 0;
  uint32_t inStable_ = 0;
  int debounce_ = 0;
  bool camShutter_ = false;
  unsigned buzzerRemainMs_ = 0;
  uint32_t lastTickMs_ = 0;
};

}  // namespace dfdmc
