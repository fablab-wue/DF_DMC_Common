#pragma once

// Live DMX512 (PIO UART, 250000 8N2 plus BREAK/MAB) and an optional
// high-active PWM mirror of the first channels. The wire levels stay raw.

#include "dmc_protocol.h"

#include <SerialPIO.h>
#include <cstdint>

namespace dfdmc {

class DmxEngine {
 public:
  void begin(uint8_t txPin, const uint8_t* pwmPins, uint8_t pwmCount, uint32_t pwmHz, bool exponential);
  bool ramping() const { return ramping_; }
  void apply(uint16_t startChannel, const uint8_t* levels, uint16_t count, bool ramp);
  void update();

 private:
  void sendNow();
  void beginPwm();
  void writePwm();
  uint8_t pwmLevel(uint8_t channelLevel) const;
  SerialPIO& uart();

  alignas(SerialPIO) unsigned char uartMem_[sizeof(SerialPIO)]{};
  bool uartLive_ = false;
  uint8_t txPin_ = 0;
  const uint8_t* pwmPins_ = nullptr;
  uint8_t pwmCount_ = 0;
  uint32_t pwmHz_ = 18000;
  bool exponential_ = false;
  uint8_t current_[kDmxChannels]{};
  uint8_t target_[kDmxChannels]{};
  uint8_t start_[kDmxChannels]{};
  bool ramping_ = false;
  bool txReady_ = false;
  uint32_t rampStartMs_ = 0;
  uint32_t lastSendMs_ = 0;
};

}  // namespace dfdmc
