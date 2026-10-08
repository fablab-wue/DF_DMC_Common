#include "rt_support.h"

#include <Arduino.h>

namespace dfdmc {

void BloopOut::arm(uint32_t location, uint16_t dmxChannel, uint16_t timeMs) {
  clear();
  gioMask_ = location & (kDmcBloopGio0 | kDmcBloopGio1);
  if (dmxChannel >= 1 && dmxChannel <= kDmxChannels) {
    dmxChannel_ = dmxChannel;
  }
  if (gioMask_ == 0 && dmxChannel_ == 0) {
    return;
  }
  holdMs_ = timeMs == 0 ? 100 : timeMs;
}

void BloopOut::clear() {
  gioMask_ = 0;
  dmxChannel_ = 0;
  holdMs_ = 0;
  running_ = false;
  dmxOn_ = false;
  savedLevel_ = 0;
  untilMs_ = 0;
}

void BloopOut::fire(DmcGio& gio, DmxEngine& dmx) {
  if (!armed() || running_) {
    return;
  }
  running_ = true;
  untilMs_ = millis() + holdMs_;
  if (gioMask_ != 0) {
    gio.setOutputs(gio.outputs() | gioMask_);
  }
  if ((gioMask_ & kDmcBloopGio0) != 0) {
    gio.pulseBuzzer(holdMs_);
  }
  if (dmxChannel_ != 0) {
    savedLevel_ = dmx.levelAt(dmxChannel_);
    const uint8_t full = 255;
    dmx.apply(dmxChannel_, &full, 1, false);
    dmxOn_ = true;
  }
}

void BloopOut::release(DmcGio& gio, DmxEngine& dmx) {
  if (running_) {
    untilMs_ = millis();
    tick(gio, dmx);
  }
  clear();
}

void BloopOut::tick(DmcGio& gio, DmxEngine& dmx) {
  if (!running_) {
    return;
  }
  if (static_cast<int32_t>(millis() - untilMs_) < 0) {
    return;
  }
  if (gioMask_ != 0) {
    gio.setOutputs(gio.outputs() & ~gioMask_);
  }
  if (dmxOn_) {
    dmx.apply(dmxChannel_, &savedLevel_, 1, false);
    dmxOn_ = false;
  }
  running_ = false;
  holdMs_ = 0;
}

void LiveShutter::clear() {
  flags_ = 0;
  open_ = 0;
  close_ = 0;
  frameUs_ = 41667;
  videoOn_ = false;
  inFrame_ = false;
  shutter_ = false;
  openAtUs_ = 0;
  closeAtUs_ = 0;
}

void LiveShutter::arm(uint16_t flags, int16_t openAngle, int16_t closeAngle, uint32_t frameUs) {
  clear();
  flags_ = flags;
  open_ = openAngle;
  close_ = closeAngle;
  if (frameUs >= 1000) {
    frameUs_ = frameUs;
  }
}

void LiveShutter::setShutter(DmcGio& gio, bool on) {
  shutter_ = on;
  gio.setCameraShutter(on);
}

void LiveShutter::beginMove(DmcGio& gio) {
  videoOn_ = false;
  inFrame_ = false;
  if ((flags_ & kDmcRtFlagCameraStills) != 0) {
    return;
  }
  if ((flags_ & kDmcRtFlagCameraVideo) != 0) {
    videoOn_ = true;
    setShutter(gio, true);
  }
}

void LiveShutter::beginFrame(DmcGio& gio) {
  if ((flags_ & kDmcRtFlagCameraStills) == 0) {
    return;
  }
  int16_t open = open_;
  int16_t close = close_;
  if (close <= open) {
    open = 90;
    close = 180;
  }
  const uint32_t now = micros();
  const auto at = [&](int16_t angle) -> uint32_t {
    if (angle <= 0) {
      return 0;
    }
    if (angle >= 360) {
      return frameUs_;
    }
    return static_cast<uint32_t>((static_cast<uint64_t>(frameUs_) * static_cast<uint32_t>(angle)) / 360u);
  };
  openAtUs_ = now + at(open);
  closeAtUs_ = now + at(close);
  inFrame_ = closeAtUs_ != openAtUs_;
  if (inFrame_ && static_cast<int32_t>(now - openAtUs_) >= 0) {
    setShutter(gio, true);
  } else if (shutter_) {
    setShutter(gio, false);
  }
}

void LiveShutter::tick(DmcGio& gio) {
  if (!inFrame_) {
    return;
  }
  const uint32_t now = micros();
  if (!shutter_ && static_cast<int32_t>(now - openAtUs_) >= 0) {
    setShutter(gio, true);
  }
  if (shutter_ && static_cast<int32_t>(now - closeAtUs_) >= 0) {
    setShutter(gio, false);
    inFrame_ = false;
  }
}

void LiveShutter::endMove(DmcGio& gio) {
  inFrame_ = false;
  videoOn_ = false;
  if (shutter_) {
    setShutter(gio, false);
  }
  flags_ = 0;
}

}  // namespace dfdmc
