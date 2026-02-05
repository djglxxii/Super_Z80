#include "devices/apu/AudioRingBuffer.h"

#include <algorithm>
#include <cstring>

namespace sz::apu {

AudioRingBuffer::AudioRingBuffer(int capacity_frames)
    : capacity_(capacity_frames),
      buffer_(static_cast<size_t>(capacity_frames) * 2, 0) {
}

int AudioRingBuffer::FillFrames() const {
  int w = write_pos_.load(std::memory_order_acquire);
  int r = read_pos_.load(std::memory_order_acquire);
  int fill = w - r;
  if (fill < 0) fill += capacity_;
  return fill;
}

int AudioRingBuffer::Push(const int16_t* interleaved_lr, int frames) {
  int w = write_pos_.load(std::memory_order_relaxed);
  int r = read_pos_.load(std::memory_order_acquire);

  int available = capacity_ - 1 - ((w - r + capacity_) % capacity_);
  int to_write = std::min(frames, available);
  if (to_write <= 0) return 0;

  int first = std::min(to_write, capacity_ - w);
  std::memcpy(&buffer_[static_cast<size_t>(w) * 2], interleaved_lr,
              static_cast<size_t>(first) * 2 * sizeof(int16_t));

  int second = to_write - first;
  if (second > 0) {
    std::memcpy(&buffer_[0], interleaved_lr + first * 2,
                static_cast<size_t>(second) * 2 * sizeof(int16_t));
  }

  int new_w = (w + to_write) % capacity_;
  write_pos_.store(new_w, std::memory_order_release);
  return to_write;
}

int AudioRingBuffer::Pop(int16_t* out_interleaved_lr, int frames) {
  int r = read_pos_.load(std::memory_order_relaxed);
  int w = write_pos_.load(std::memory_order_acquire);

  int available = (w - r + capacity_) % capacity_;
  int to_read = std::min(frames, available);
  if (to_read <= 0) return 0;

  int first = std::min(to_read, capacity_ - r);
  std::memcpy(out_interleaved_lr, &buffer_[static_cast<size_t>(r) * 2],
              static_cast<size_t>(first) * 2 * sizeof(int16_t));

  int second = to_read - first;
  if (second > 0) {
    std::memcpy(out_interleaved_lr + first * 2, &buffer_[0],
                static_cast<size_t>(second) * 2 * sizeof(int16_t));
  }

  int new_r = (r + to_read) % capacity_;
  read_pos_.store(new_r, std::memory_order_release);
  return to_read;
}

}  // namespace sz::apu
