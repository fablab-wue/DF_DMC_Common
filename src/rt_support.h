#pragma once

// Live-move bloop and camera shutter shared by the DMC firmwares.

#include "dmc_protocol.h"
#include "dmx_engine.h"
#include "gio_io.h"

namespace dfdmc {

class BloopOut {
 public:
  void arm(uint32_t location, uint16_t dmxChannel, uint16_t timeMs);
  void clear();
  bool armed() const { return holdMs_ > 0; }
  bool running() const { return running_; }
  uint32_t heldOutputs() const { return running_ ? gioMask_ : 0; }
  unsigned holdMs() const { return holdMs_; }
  void fire(DmcGio& gio, DmxEngine& dmx);
  void tick(DmcGio& gio, DmxEngine& dmx);
  void release(DmcGio& gio, DmxEngine& dmx);

 private:
  uint32_t gioMask_ = 0;
  uint16_t dmxChannel_ = 0;
  unsigned holdMs_ = 0;
  bool running_ = false;
  bool dmxOn_ = false;
  uint8_t savedLevel_ = 0;
  uint32_t untilMs_ = 0;
};

class LiveShutter {
 public:
  void clear();
  void arm(uint16_t flags, int16_t openAngle, int16_t closeAngle, uint32_t frameUs);
  void beginMove(DmcGio& gio);
  void beginFrame(DmcGio& gio);
  void tick(DmcGio& gio);
  void endMove(DmcGio& gio);

 private:
  void setShutter(DmcGio& gio, bool on);

  uint16_t flags_ = 0;
  int16_t open_ = 0;
  int16_t close_ = 0;
  uint32_t frameUs_ = 41667;
  bool videoOn_ = false;
  bool inFrame_ = false;
  bool shutter_ = false;
  uint32_t openAtUs_ = 0;
  uint32_t closeAtUs_ = 0;
};

}  // namespace dfdmc
