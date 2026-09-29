#pragma once

// Live DMX512 (PIO UART, 250000 8N2, BREAK 176 µs, MAB 24 µs) and an
// optional high-active PWM mirror of the first channels. A hardware
// timer keeps the 512-slot universe on the wire at the full frame rate.
// Wire levels stay raw.

#include "dmc_protocol.h"

#include <SerialPIO.h>
#include <pico/time.h>
#include <cstdint>

namespace dfdmc {

class DmxEngine {
 public:
  void begin(uint8_t txPin, const uint8_t* pwmPins, uint8_t pwmCount, uint32_t pwmHz, bool exponential);
  bool ramping() const { return ramping_; }
  uint8_t levelAt(uint16_t channel1) const;
  void apply(uint16_t startChannel, const uint8_t* levels, uint16_t count, bool ramp);
  void update();

 private:
  enum class TxState : uint8_t { Idle, Break, Mab, Data, Tail };

  static bool onTxTimer(repeating_timer_t* timer);
  void onTick();
  void publish(const uint8_t* levels);
  void startFrame();
  void feedFifo();
  void serviceTx();
  void beginPwm();
  void writePwm();
  uint16_t pwmLevel(uint8_t channelLevel) const;
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
  uint8_t frame_[kDmxChannels]{};
  bool ramping_ = false;
  bool txReady_ = false;
  bool startSent_ = false;
  bool txTimerOn_ = false;
  TxState txState_ = TxState::Idle;
  uint16_t slot_ = 0;
  uint32_t rampStartMs_ = 0;
  uint32_t markUs_ = 0;
  uint32_t tailIdleUs_ = 0;
  repeating_timer_t txTimer_{};
};

}  // namespace dfdmc
