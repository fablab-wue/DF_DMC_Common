#include "dmx_engine.h"

#include <hardware/clocks.h>
#include <hardware/gpio.h>
#include <hardware/pwm.h>

#include <cstring>
#include <new>

namespace dfdmc {

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
  lastSendMs_ = 0;
  if (!uartLive_) {
    new (uartMem_) SerialPIO(txPin_, NOPIN);
    uartLive_ = true;
  }
  uart().begin(250000, SERIAL_8N2);
  txReady_ = static_cast<bool>(uart());
  beginPwm();
  writePwm();
}

uint8_t DmxEngine::pwmLevel(uint8_t channelLevel) const {
  if (!exponential_) {
    return channelLevel;
  }
  return static_cast<uint8_t>((static_cast<uint16_t>(channelLevel) * channelLevel) / 255u);
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
  if (!ramp) {
    memcpy(current_, target_, sizeof(current_));
    ramping_ = false;
    sendNow();
    return;
  }
  rampStartMs_ = millis();
  ramping_ = true;
}

void DmxEngine::update() {
  const uint32_t now = millis();
  if (!ramping_) {
    return;
  }
  const uint32_t elapsed = now - rampStartMs_;
  constexpr uint32_t kRampMs = 500;
  float t = static_cast<float>(elapsed) / static_cast<float>(kRampMs);
  if (t >= 1.0f) {
    memcpy(current_, target_, sizeof(current_));
    ramping_ = false;
    sendNow();
    return;
  }
  for (int i = 0; i < kDmxChannels; ++i) {
    const int a = start_[i];
    const int b = target_[i];
    current_[i] = static_cast<uint8_t>(a + static_cast<int>((b - a) * t));
  }
  if (now - lastSendMs_ >= 25) {
    sendNow();
  }
}

void DmxEngine::beginPwm() {
  if (pwmCount_ == 0 || pwmPins_ == nullptr) {
    return;
  }
  const float div = static_cast<float>(clock_get_hz(clk_sys)) / (static_cast<float>(pwmHz_) * 255.0f);
  pwm_config cfg = pwm_get_default_config();
  pwm_config_set_clkdiv(&cfg, div);
  pwm_config_set_wrap(&cfg, 254);
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

void DmxEngine::sendNow() {
  writePwm();
  if (!txReady_ || !uartLive_) {
    return;
  }
  uart().flush();
  uart().end();
  pinMode(txPin_, OUTPUT);
  digitalWrite(txPin_, LOW);
  delayMicroseconds(88);
  digitalWrite(txPin_, HIGH);
  delayMicroseconds(12);
  uart().begin(250000, SERIAL_8N2);
  txReady_ = static_cast<bool>(uart());
  if (!txReady_) {
    return;
  }
  uart().write(static_cast<uint8_t>(0));
  uart().write(current_, kDmxChannels);
  lastSendMs_ = millis();
}

}  // namespace dfdmc
