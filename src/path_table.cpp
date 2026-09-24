#include "path_table.h"

#include <cstring>

namespace dfdmc {

void PathTable::beginUpload(int32_t startFrame, int32_t endFrame, int axisCount) {
  startFrame_ = startFrame < 1 ? 1 : startFrame;
  endFrame_ = endFrame < startFrame_ ? startFrame_ : endFrame;
  frameCount_ = endFrame_ - startFrame_ + 1;
  if (frameCount_ > kMaxUploadFrames) {
    frameCount_ = kMaxUploadFrames;
    endFrame_ = startFrame_ + frameCount_ - 1;
  }
  if (axisCount < 0) {
    axisCount = 0;
  }
  if (axisCount > kMaxAxes) {
    axisCount = kMaxAxes;
  }
  axisCount_ = axisCount;
  memset(pos_, 0, sizeof(pos_));
  memset(triggers_, 0, sizeof(triggers_));
  triggerMask_ = 0;
  ready_ = false;
}

bool PathTable::storeAxis(int motor1, uint32_t index, const int32_t* values, int n, bool finalFill) {
  if (motor1 < 1 || motor1 > axisCount_ || frameCount_ <= 0 || values == nullptr) {
    return false;
  }
  int idx = static_cast<int>(index);
  if (idx < 0 || idx >= frameCount_) {
    return false;
  }
  const int axis = motor1 - 1;
  for (int i = 0; i < n && idx < frameCount_; ++i, ++idx) {
    pos_[axis][idx] = values[i];
  }
  if (finalFill && idx > 0) {
    const int32_t last = pos_[axis][idx - 1];
    while (idx < frameCount_) {
      pos_[axis][idx++] = last;
    }
  }
  return true;
}

void PathTable::storeTrigger(uint32_t mask, uint32_t index, uint32_t value) {
  triggerMask_ = mask;
  if (index < static_cast<uint32_t>(frameCount_)) {
    triggers_[index] = static_cast<uint8_t>(value & 0x0FU);
  }
}

bool PathTable::finishUpload() {
  ready_ = frameCount_ > 0;
  return ready_;
}

int32_t PathTable::positionSteps(int axis0, int localFrame) const {
  if (axis0 < 0 || axis0 >= axisCount_ || localFrame < 0 || localFrame >= frameCount_) {
    return 0;
  }
  return pos_[axis0][localFrame];
}

bool PathTable::localFrame(int dfFrame, int* out) const {
  if (out == nullptr || dfFrame < startFrame_ || dfFrame > endFrame_) {
    return false;
  }
  *out = dfFrame - startFrame_;
  return true;
}

uint8_t PathTable::triggerAtLocal(int localFrame) const {
  if (localFrame < 0 || localFrame >= frameCount_) {
    return 0;
  }
  return triggers_[localFrame];
}

}  // namespace dfdmc
