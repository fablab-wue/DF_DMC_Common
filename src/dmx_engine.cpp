#include "dmx_engine.h"

#include <hardware/clocks.h>
#include <hardware/gpio.h>
#include <hardware/pwm.h>

#include <cstring>
#include <new>

namespace dfdmc {

// E1.11 transmitter limits: BREAK >= 92 µs (typical 176), MAB >= 12 µs.
// The PIO UART stays up. BREAK is a GPIO override so the line is not
// glitched by tearing the state machine down between frames.
constexpr uint32_t kBreakUs = 176;
constexpr uint32_t kMabUs = 24;
// FIFO-empty still leaves the last slot in the shifter (~48 µs at 12 bits).
constexpr uint32_t kTailUs = 64;
// 8-deep TX FIFO holds ~384 µs. Refill well inside that, from a timer,
// so a slow main loop during realtime play cannot stretch the universe.
constexpr int64_t kTxTickUs = 150;
// Short packets would otherwise repeat about every millisecond.
constexpr uint32_t kFramePeriodUs = 25000;

SerialPIO& DmxEngine::uart() { return *reinterpret_cast<SerialPIO*>(uartMem_); }

void DmxEngine::begin(uint8_t txPin, const uint8_t* pwmPins, uint8_t pwmCount, uint32_t pwmHz, bool exponential) {
  txPin_ = txPin;
  pwmPins_ = pwmPins;
  pwmCount_ = pwmPins == nullptr ? 0 : pwmCount;
  pwmHz_ = pwmHz == 0 ? 18000 : pwmHz;
  exponential_ = exponential;
  memset(current_, 0, sizeof(current_));
  memset(target_, 0, sizeof(target_));
  memset(start_, 0, sizeof(start_));
  ramping_ = false;
  txState_ = TxState::Idle;
  txSlots_ = 0;
  frameSlots_ = 0;
  frameStartUs_ = 0;
  if (!uartLive_) {
    new (uartMem_) SerialPIO(txPin_, NOPIN);
    uartLive_ = true;
  }
  uart().begin(250000, SERIAL_8N2);
  txReady_ = static_cast<bool>(uart());
  if (txReady_ && !txTimerOn_) {
    txTimerOn_ = add_repeating_timer_us(-kTxTickUs, &DmxEngine::onTxTimer, this, &txTimer_);
  }
  beginPwm();
  writePwm();
}

bool DmxEngine::onTxTimer(repeating_timer_t* timer) {
  static_cast<DmxEngine*>(timer->user_data)->onTick();
  return true;
}

void DmxEngine::publish(const uint8_t* levels) {
  noInterrupts();
  memcpy(current_, levels, sizeof(current_));
  interrupts();
}

void DmxEngine::noteSlots(uint16_t startChannel, const uint8_t* levels, uint16_t count) {
  uint16_t high = txSlots_;
  for (uint16_t i = 0; i < count; ++i) {
    if (levels[i] == 0) {
      continue;
    }
    const uint16_t channel = static_cast<uint16_t>(startChannel + i);
    if (channel > high) {
      high = channel;
    }
  }
  if (high > txSlots_) {
    txSlots_ = high;
  }
}

uint8_t DmxEngine::levelAt(uint16_t channel1) const {
  if (channel1 < 1 || channel1 > kDmxChannels) {
    return 0;
  }
  return current_[channel1 - 1];
}

uint16_t DmxEngine::pwmLevel(uint8_t channelLevel) const {
  if (!exponential_) {
    return channelLevel;
  }
  const uint16_t level = channelLevel;
  return static_cast<uint16_t>(level * level);
}

void DmxEngine::apply(uint16_t startChannel, const uint8_t* levels, uint16_t count, bool ramp) {
  if (startChannel < 1) {
    startChannel = 1;
  }
  const int start = static_cast<int>(startChannel - 1);
  if (start >= kDmxChannels || levels == nullptr || count == 0) {
    return;
  }
  if (start + count > kDmxChannels) {
    count = static_cast<uint16_t>(kDmxChannels - start);
  }
  memcpy(start_, current_, sizeof(current_));
  memcpy(target_, current_, sizeof(target_));
  memcpy(target_ + start, levels, count);
  noteSlots(startChannel, levels, count);
  if (!ramp) {
    publish(target_);
    ramping_ = false;
    writePwm();
    return;
  }
  rampStartMs_ = millis();
  ramping_ = true;
}

void DmxEngine::update() {
  if (ramping_) {
    const uint32_t elapsed = millis() - rampStartMs_;
    constexpr uint32_t kRampMs = 500;
    float t = static_cast<float>(elapsed) / static_cast<float>(kRampMs);
    if (t >= 1.0f) {
      publish(target_);
      ramping_ = false;
    } else {
      uint8_t stepped[kDmxChannels];
      for (int i = 0; i < kDmxChannels; ++i) {
        const int a = start_[i];
        const int b = target_[i];
        stepped[i] = static_cast<uint8_t>(a + static_cast<int>((b - a) * t));
      }
      publish(stepped);
    }
    writePwm();
  }
  if (!txTimerOn_) {
    serviceTx();
  }
}

void DmxEngine::onTick() {
  const uint32_t now = micros();
  switch (txState_) {
    case TxState::Idle:
      if (txSlots_ == 0 || static_cast<uint32_t>(now - frameStartUs_) < kFramePeriodUs) {
        break;
      }
      gpio_set_outover(txPin_, GPIO_OVERRIDE_LOW);
      frameStartUs_ = now;
      markUs_ = now;
      txState_ = TxState::Break;
      break;
    case TxState::Break:
      if (static_cast<uint32_t>(now - markUs_) >= kBreakUs) {
        gpio_set_outover(txPin_, GPIO_OVERRIDE_NORMAL);
        markUs_ = now;
        txState_ = TxState::Mab;
      }
      break;
    case TxState::Mab:
      if (static_cast<uint32_t>(now - markUs_) >= kMabUs) {
        frameSlots_ = txSlots_;
        if (frameSlots_ > kDmxChannels) {
          frameSlots_ = kDmxChannels;
        }
        memcpy(frame_, current_, frameSlots_);
        slot_ = 0;
        startSent_ = false;
        txState_ = TxState::Data;
        feedFifo();
      }
      break;
    case TxState::Data:
      feedFifo();
      break;
    case TxState::Tail:
      if (uart().availableForWrite() < 8) {
        tailIdleUs_ = 0;
        break;
      }
      if (tailIdleUs_ == 0) {
        tailIdleUs_ = now == 0 ? 1 : now;
        break;
      }
      if (static_cast<uint32_t>(now - tailIdleUs_) >= kTailUs) {
        txState_ = TxState::Idle;
      }
      break;
  }
}

void DmxEngine::beginPwm() {
  if (pwmCount_ == 0 || pwmPins_ == nullptr) {
    return;
  }
  const uint32_t top = exponential_ ? (254u * 255u) : 254u;
  const float cycles = static_cast<float>(top + 1u);
  float div = static_cast<float>(clock_get_hz(clk_sys)) / (static_cast<float>(pwmHz_) * cycles);
  if (div < 1.0f) {
    div = 1.0f;
  }
  if (div > 256.0f) {
    div = 256.0f;
  }
  pwm_config cfg = pwm_get_default_config();
  pwm_config_set_clkdiv(&cfg, div);
  pwm_config_set_wrap(&cfg, static_cast<uint16_t>(top));
  bool sliceInited[8] = {};
  for (uint8_t i = 0; i < pwmCount_; ++i) {
    const uint8_t pin = pwmPins_[i];
    gpio_set_function(pin, GPIO_FUNC_PWM);
    const unsigned slice = pwm_gpio_to_slice_num(pin);
    if (slice < 8 && !sliceInited[slice]) {
      pwm_init(slice, &cfg, true);
      sliceInited[slice] = true;
    }
    pwm_set_gpio_level(pin, 0);
  }
}

void DmxEngine::writePwm() {
  if (pwmCount_ == 0 || pwmPins_ == nullptr) {
    return;
  }
  for (uint8_t i = 0; i < pwmCount_; ++i) {
    pwm_set_gpio_level(pwmPins_[i], pwmLevel(current_[i]));
  }
}

void DmxEngine::startFrame() {
  const uint32_t now = micros();
  if (txSlots_ == 0 || static_cast<uint32_t>(now - frameStartUs_) < kFramePeriodUs) {
    return;
  }
  frameStartUs_ = now;
  frameSlots_ = txSlots_;
  if (frameSlots_ > kDmxChannels) {
    frameSlots_ = kDmxChannels;
  }
  noInterrupts();
  memcpy(frame_, current_, frameSlots_);
  interrupts();
  gpio_set_outover(txPin_, GPIO_OVERRIDE_LOW);
  delayMicroseconds(kBreakUs);
  gpio_set_outover(txPin_, GPIO_OVERRIDE_NORMAL);
  delayMicroseconds(kMabUs);
  slot_ = 0;
  startSent_ = false;
  txState_ = TxState::Data;
  feedFifo();
}

void DmxEngine::feedFifo() {
  while (uart().availableForWrite() > 0) {
    if (!startSent_) {
      if (uart().write(static_cast<uint8_t>(0)) != 1) {
        return;
      }
      startSent_ = true;
      continue;
    }
    if (slot_ >= frameSlots_) {
      txState_ = TxState::Tail;
      tailIdleUs_ = 0;
      return;
    }
    if (uart().write(frame_[slot_]) != 1) {
      return;
    }
    ++slot_;
  }
}

void DmxEngine::serviceTx() {
  if (!txReady_ || !uartLive_) {
    return;
  }
  if (txState_ == TxState::Data) {
    feedFifo();
    return;
  }
  if (txState_ == TxState::Tail) {
    if (uart().availableForWrite() < 8) {
      tailIdleUs_ = 0;
      return;
    }
    const uint32_t now = micros();
    if (tailIdleUs_ == 0) {
      tailIdleUs_ = now;
      return;
    }
    if (static_cast<uint32_t>(now - tailIdleUs_) < kTailUs) {
      return;
    }
  }
  startFrame();
}

}  // namespace dfdmc
