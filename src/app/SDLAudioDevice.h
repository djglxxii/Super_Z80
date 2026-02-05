#ifndef SUPERZ80_APP_SDLAUDIODEVICE_H
#define SUPERZ80_APP_SDLAUDIODEVICE_H

#include <SDL.h>
#include <cstdint>

namespace sz::apu {
class APU;
}

namespace sz::app {

struct SDLAudioSpecOut {
  int sample_rate = 0;
  int channels = 0;
  int samples_per_callback = 0;
};

class SDLAudioDevice {
 public:
  bool Open(sz::apu::APU* apu, int requested_sample_rate, SDLAudioSpecOut& out_obtained);
  void Close();
  void Start();
  void Stop();
  bool IsOpen() const { return device_id_ != 0; }

 private:
  static void SDLCALL AudioCallback(void* userdata, uint8_t* stream, int len);

  SDL_AudioDeviceID device_id_ = 0;
  sz::apu::APU* apu_ = nullptr;
};

}  // namespace sz::app

#endif
