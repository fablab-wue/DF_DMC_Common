#include "dmc_protocol.h"

#include <Arduino.h>

#include <cmath>

namespace dfdmc {

uint16_t computeChecksum(const uint8_t* data, size_t bytes) {
  uint16_t sum1 = 0;
  uint16_t sum2 = 0;
  size_t remaining = bytes;
  while (remaining > 0) {
    const size_t chunk = (remaining > 20) ? 20 : remaining;
    remaining -= chunk;
    for (size_t i = 0; i < chunk; ++i) {
      sum2 += sum1 += data[i];
    }
    data += chunk;
    sum1 %= 0xFF;
    sum2 %= 0xFF;
  }
  return (sum2 << 8) | sum1;
}

uint16_t encodeChecksum(uint16_t rawChecksum) {
  const uint8_t low = static_cast<uint8_t>(rawChecksum & 0xFFU);
  const uint8_t high = static_cast<uint8_t>((rawChecksum >> 8) & 0xFFU);
  const uint8_t c0 = static_cast<uint8_t>(0xFFU - ((low + high) % 0xFFU));
  const uint8_t c1 = static_cast<uint8_t>(0xFFU - ((low + c0) % 0xFFU));
  return static_cast<uint16_t>(c1 << 8) | static_cast<uint16_t>(c0);
}

void appendByte(std::vector<uint8_t>& out, uint8_t value) { out.push_back(value); }

void appendWordLE(std::vector<uint8_t>& out, uint16_t value) {
  out.push_back(static_cast<uint8_t>(value & 0xFFU));
  out.push_back(static_cast<uint8_t>((value >> 8) & 0xFFU));
}

void appendDwordLE(std::vector<uint8_t>& out, uint32_t value) {
  out.push_back(static_cast<uint8_t>(value & 0xFFU));
  out.push_back(static_cast<uint8_t>((value >> 8) & 0xFFU));
  out.push_back(static_cast<uint8_t>((value >> 16) & 0xFFU));
  out.push_back(static_cast<uint8_t>((value >> 24) & 0xFFU));
}

uint16_t decodeWordLE(const uint8_t* ptr) {
  return static_cast<uint16_t>(ptr[0]) | (static_cast<uint16_t>(ptr[1]) << 8);
}

uint32_t decodeDwordLE(const uint8_t* ptr) {
  return static_cast<uint32_t>(ptr[0]) | (static_cast<uint32_t>(ptr[1]) << 8) |
         (static_cast<uint32_t>(ptr[2]) << 16) | (static_cast<uint32_t>(ptr[3]) << 24);
}

bool readByte(const std::vector<uint8_t>& payload, size_t offset, uint8_t* value) {
  if (offset >= payload.size() || value == nullptr) {
    return false;
  }
  *value = payload[offset];
  return true;
}

bool readWordLE(const std::vector<uint8_t>& payload, size_t offset, uint16_t* value) {
  if (offset + 2 > payload.size() || value == nullptr) {
    return false;
  }
  *value = decodeWordLE(payload.data() + offset);
  return true;
}

bool readDwordLE(const std::vector<uint8_t>& payload, size_t offset, uint32_t* value) {
  if (offset + 4 > payload.size() || value == nullptr) {
    return false;
  }
  *value = decodeDwordLE(payload.data() + offset);
  return true;
}

bool readSignedDwordLE(const std::vector<uint8_t>& payload, size_t offset, int32_t* value) {
  uint32_t raw = 0;
  if (!readDwordLE(payload, offset, &raw) || value == nullptr) {
    return false;
  }
  *value = static_cast<int32_t>(raw);
  return true;
}

bool parseRtRunMove(const std::vector<uint8_t>& payload, RtRunMove* out) {
  if (out == nullptr || payload.size() < 12) {
    return false;
  }
  *out = RtRunMove{};
  if (!readDwordLE(payload, 0, &out->fpsX1000) || !readSignedDwordLE(payload, 4, &out->startFrame) ||
      !readSignedDwordLE(payload, 8, &out->endFrame)) {
    return false;
  }
  if (out->fpsX1000 < 1) {
    out->fpsX1000 = 24000;
  }
  if (payload.size() >= 16) {
    readDwordLE(payload, 12, &out->prerollMs);
  }
  if (payload.size() >= 20) {
    readDwordLE(payload, 16, &out->postrollMs);
  }
  if (payload.size() >= 21) {
    uint8_t sync = 0;
    readByte(payload, 20, &sync);
    out->syncDmx = sync != 0;
  }
  if (payload.size() >= 25) {
    readDwordLE(payload, 21, &out->bloopLocation);
  }
  if (payload.size() >= 27) {
    readWordLE(payload, 25, &out->bloopDmx);
  }
  if (payload.size() >= 29) {
    readWordLE(payload, 27, &out->bloopTimeMs);
  }
  if (payload.size() >= 31) {
    readWordLE(payload, 29, &out->flags);
  }
  if ((out->flags & kDmcRtFlagCameraStills) != 0 && payload.size() >= 35) {
    uint16_t openWord = 0;
    uint16_t closeWord = 0;
    readWordLE(payload, 31, &openWord);
    readWordLE(payload, 33, &closeWord);
    out->cameraOpen = static_cast<int16_t>(openWord);
    out->cameraClose = static_cast<int16_t>(closeWord);
    out->hasCameraAngles = true;
  }
  return true;
}

void rollFrameTimes(const RtRunMove& move, double* prerollFrame, double* postrollFrame) {
  const double dir = move.startFrame <= move.endFrame ? 1.0 : -1.0;
  const double scale = static_cast<double>(move.fpsX1000) / 1000000.0;
  // Rest-to-cruise accel covers half the distance cruise would cover in that time.
  const double pre = dir * 0.5 * static_cast<double>(move.prerollMs) * scale;
  const double post = dir * 0.5 * static_cast<double>(move.postrollMs) * scale;
  if (prerollFrame != nullptr) {
    *prerollFrame = static_cast<double>(move.startFrame) - pre;
  }
  if (postrollFrame != nullptr) {
    *postrollFrame = static_cast<double>(move.endFrame) + post;
  }
}

void rtPlaySpan(const RtRunMove& move, RtPlaySpan* out) {
  if (out == nullptr) {
    return;
  }
  *out = RtPlaySpan{};
  rollFrameTimes(move, &out->prerollFrame, &out->postrollFrame);
  const bool forward = move.startFrame <= move.endFrame;
  if (forward) {
    out->playFrom = static_cast<int>(std::floor(out->prerollFrame));
    out->playTo = static_cast<int>(std::ceil(out->postrollFrame));
  } else {
    out->playFrom = static_cast<int>(std::ceil(out->prerollFrame));
    out->playTo = static_cast<int>(std::floor(out->postrollFrame));
  }
}

uint32_t framePeriodUs(uint32_t fpsX1000) {
  if (fpsX1000 < 1) {
    fpsX1000 = 24000;
  }
  uint32_t us = (1000000000UL + (fpsX1000 / 2)) / fpsX1000;
  if (us < 1000) {
    us = 1000;
  }
  return us;
}

uint32_t moveTimeThousandths(int32_t frame) {
  const int64_t scaled = static_cast<int64_t>(frame) * 1000;
  return static_cast<uint32_t>(static_cast<int32_t>(scaled));
}

uint32_t softLimitFault(bool lowerEn, int32_t lower, bool upperEn, int32_t upper, int32_t steps) {
  if (lowerEn && steps < lower) {
    return kDmcAckErrSoftLow;
  }
  if (upperEn && steps > upper) {
    return kDmcAckErrSoftUp;
  }
  return 0;
}

LiveDmxStatus parseLiveDmx(const std::vector<uint8_t>& payload, LiveDmx* out) {
  if (out == nullptr || payload.size() < 4) {
    return LiveDmxStatus::kGeneral;
  }
  const uint16_t channel = decodeWordLE(payload.data() + 1);
  const uint16_t count = static_cast<uint16_t>(payload.size() - 3);
  if (count == 0 || channel < 1 || channel > kDmxChannels ||
      static_cast<uint32_t>(channel) + count - 1 > static_cast<uint32_t>(kDmxChannels)) {
    return LiveDmxStatus::kRange;
  }
  out->ramp = payload[0] != 0;
  out->channel = channel;
  out->count = count;
  out->levels = payload.data() + 3;
  return LiveDmxStatus::kOk;
}

bool writeUsbFrame(const uint8_t* data, size_t size) {
  if (data == nullptr || size == 0) {
    return true;
  }
  if (!Serial) {
    return false;
  }
  const uint32_t deadline = millis() + 30;
  size_t off = 0;
  while (off < size) {
    const int space = Serial.availableForWrite();
    if (space > 0) {
      size_t n = size - off;
      if (n > static_cast<size_t>(space)) {
        n = static_cast<size_t>(space);
      }
      const size_t wrote = Serial.write(data + off, n);
      if (wrote > 0) {
        off += wrote;
        continue;
      }
    }
    if (static_cast<int32_t>(millis() - deadline) >= 0) {
      return false;
    }
    yield();
  }
  return true;
}

DmcParser::DmcParser() { reset(); }

void DmcParser::reset() {
  state_ = State::kStart;
  lastByte_ = 0;
  index_ = 0;
  buffer_.fill(0);
  buffer_[0] = 'D';
  buffer_[1] = 'F';
}

bool DmcParser::feed(uint8_t byte, DmcFrame* frame) {
  if (frame != nullptr) {
    frame->id = 0;
    frame->type = 0;
    frame->length = 0;
    frame->payload.clear();
    frame->valid = false;
  }

  switch (state_) {
    case State::kStart:
      if (byte == 'F' && lastByte_ == 'D') {
        buffer_[0] = 'D';
        buffer_[1] = 'F';
        index_ = 2;
        lastByte_ = 0;
        state_ = State::kHeader;
      } else {
        lastByte_ = byte;
      }
      return false;

    case State::kHeader:
      buffer_[index_++] = byte;
      if (index_ == kDmcHeaderSize) {
        const uint16_t length = decodeWordLE(buffer_.data() + 8);
        if (static_cast<size_t>(length) + kDmcCsumSize > kDmcMaxFrameSize - kDmcHeaderSize) {
          reset();
          return false;
        }
        state_ = State::kData;
      }
      return false;

    case State::kData:
      buffer_[index_++] = byte;
      if (index_ >= kDmcHeaderSize + static_cast<size_t>(decodeWordLE(buffer_.data() + 8)) + kDmcCsumSize) {
        state_ = State::kStart;
        if (frame == nullptr) {
          return false;
        }
        const uint16_t length = decodeWordLE(buffer_.data() + 8);
        const uint16_t type = decodeWordLE(buffer_.data() + 6);
        const uint32_t id = decodeDwordLE(buffer_.data() + 2);
        const size_t totalBytes = index_;
        const uint16_t expectedChecksum = decodeWordLE(buffer_.data() + totalBytes - kDmcCsumSize);
        const uint16_t actualChecksum = computeChecksum(buffer_.data(), totalBytes - kDmcCsumSize);
        const uint16_t encodedChecksum = encodeChecksum(actualChecksum);
        frame->id = id;
        frame->type = type;
        frame->length = length;
        frame->valid = (encodedChecksum == expectedChecksum);
        frame->payload.assign(buffer_.begin() + kDmcHeaderSize, buffer_.begin() + kDmcHeaderSize + length);
        index_ = 0;
        return true;
      }
      return false;
  }
  return false;
}

}  // namespace dfdmc
