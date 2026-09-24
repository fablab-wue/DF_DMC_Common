#pragma once

// Dragonframe realtime upload: int32 steps per frame, plus GIO triggers.
// SliderMC PD packing stays in the firmware that talks to a motion controller.

#include <cstdint>

#ifndef DFDMC_MAX_AXES
#define DFDMC_MAX_AXES 6
#endif

#ifndef DFDMC_MAX_UPLOAD_FRAMES
#define DFDMC_MAX_UPLOAD_FRAMES 2048
#endif

namespace dfdmc {

constexpr int kMaxAxes = DFDMC_MAX_AXES;
constexpr int kMaxUploadFrames = DFDMC_MAX_UPLOAD_FRAMES;

class PathTable {
 public:
  void beginUpload(int32_t startFrame, int32_t endFrame, int axisCount);
  bool storeAxis(int motor1, uint32_t index, const int32_t* values, int n, bool finalFill);
  void storeTrigger(uint32_t mask, uint32_t index, uint32_t value);
  bool finishUpload();

  bool empty() const { return !ready_ || frameCount_ <= 0; }
  int frameCount() const { return frameCount_; }
  int startFrame() const { return startFrame_; }
  int endFrame() const { return endFrame_; }
  int axisCount() const { return axisCount_; }
  uint32_t triggerMask() const { return triggerMask_; }

  int32_t positionSteps(int axis0, int localFrame) const;
  bool localFrame(int dfFrame, int* out) const;
  uint8_t triggerAtLocal(int localFrame) const;

 private:
  int32_t pos_[kMaxAxes][kMaxUploadFrames]{};
  uint8_t triggers_[kMaxUploadFrames]{};
  int frameCount_ = 0;
  int startFrame_ = 1;
  int endFrame_ = 1;
  int axisCount_ = 0;
  uint32_t triggerMask_ = 0;
  bool ready_ = false;
};

}  // namespace dfdmc
