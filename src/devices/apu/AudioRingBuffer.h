#ifndef SUPERZ80_DEVICES_APU_AUDIORINGBUFFER_H
#define SUPERZ80_DEVICES_APU_AUDIORINGBUFFER_H

#include <atomic>
#include <cstdint>
#include <vector>

namespace sz::apu {

// Lock-free single-producer/single-consumer ring buffer for stereo int16 audio.
// Stores interleaved stereo frames (L, R, L, R, ...).
// Thread-safe for one producer thread and one consumer thread.
class AudioRingBuffer {
 public:
  explicit AudioRingBuffer(int capacity_frames);

  int CapacityFrames() const { return capacity_; }
  int FillFrames() const;

  // Producer: push interleaved int16 stereo frames.
  // Returns number of frames actually pushed (may be less if buffer full).
  int Push(const int16_t* interleaved_lr, int frames);

  // Consumer: pop interleaved int16 stereo frames.
  // Returns number of frames actually popped (may be less if buffer empty).
  int Pop(int16_t* out_interleaved_lr, int frames);

 private:
  int capacity_;  // in frames
  std::vector<int16_t> buffer_;  // capacity * 2 samples (stereo)
  std::atomic<int> write_pos_{0};  // in frames
  std::atomic<int> read_pos_{0};   // in frames
};

}  // namespace sz::apu

#endif
