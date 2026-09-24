#include "gio_io.h"

namespace dfdmc {

void DmcGio::ocWrite(uint8_t pin, bool active) {
  if (pin == 255) {
    return;
  }
  if (active) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
  } else {
    pinMode(pin, INPUT_PULLUP);
  }
}

void DmcGio::begin(const GioMap& map) {
  map_ = map;
  if (map_.outPins == nullptr) {
    map_.outCount = 0;
  }
  if (map_.inPins == nullptr) {
    map_.inCount = 0;
  }
  for (uint8_t i = 0; i < map_.outCount; ++i) {
    ocWrite(map_.outPins[i], false);
  }
  for (uint8_t i = 0; i < map_.inCount; ++i) {
    pinMode(map_.inPins[i], INPUT_PULLUP);
  }
  ocWrite(map_.cameraPin, false);
  if (map_.buzzerPin != 255) {
    pinMode(map_.buzzerPin, OUTPUT);
    digitalWrite(map_.buzzerPin, LOW);
  }
  if (map_.movePin != 255) {
    pinMode(map_.movePin, OUTPUT);
    digitalWrite(map_.movePin, LOW);
  }
  outBits_ = 0;
  inBits_ = readInputs();
  inStable_ = inBits_;
  debounce_ = 0;
  camShutter_ = false;
  buzzerRemainMs_ = 0;
  lastTickMs_ = millis();
}

void DmcGio::setOutputs(uint32_t bits) {
  const uint32_t mask = map_.outCount >= 32 ? 0xFFFFFFFFu : ((1u << map_.outCount) - 1u);
  outBits_ = bits & mask;
  for (uint8_t i = 0; i < map_.outCount; ++i) {
    ocWrite(map_.outPins[i], (outBits_ & (1u << i)) != 0);
  }
}

bool DmcGio::pollInputChange() {
  const uint32_t now = readInputs();
  if (now == inBits_) {
    if (debounce_ < 5) {
      ++debounce_;
    }
    if (debounce_ >= 5 && now != inStable_) {
      inStable_ = now;
      return true;
    }
  } else {
    inBits_ = now;
    debounce_ = 0;
  }
  return false;
}

void DmcGio::setCameraShutter(bool on) {
  camShutter_ = on;
  ocWrite(map_.cameraPin, on);
}

void DmcGio::pulseBuzzer(unsigned ms) {
  if (ms == 0 || map_.buzzerPin == 255) {
    return;
  }
  digitalWrite(map_.buzzerPin, HIGH);
  buzzerRemainMs_ = ms;
}

void DmcGio::tick() {
  const uint32_t now = millis();
  const uint32_t dt = now - lastTickMs_;
  lastTickMs_ = now;
  if (buzzerRemainMs_ == 0 || map_.buzzerPin == 255) {
    return;
  }
  if (dt >= buzzerRemainMs_) {
    buzzerRemainMs_ = 0;
    digitalWrite(map_.buzzerPin, LOW);
  } else {
    buzzerRemainMs_ -= dt;
  }
}

uint32_t DmcGio::readInputs() const {
  uint32_t bits = 0;
  for (uint8_t i = 0; i < map_.inCount; ++i) {
    if (digitalRead(map_.inPins[i]) == LOW) {
      bits |= (1u << i);
    }
  }
  return bits;
}

}  // namespace dfdmc
