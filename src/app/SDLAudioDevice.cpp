#include "app/SDLAudioDevice.h"

#include "core/log/Logger.h"
#include "devices/apu/APU.h"

namespace sz::app {

bool SDLAudioDevice::Open(sz::apu::APU* apu, int requested_sample_rate,
                          SDLAudioSpecOut& out_obtained) {
  apu_ = apu;

  SDL_AudioSpec want{};
  want.freq = requested_sample_rate;
  want.format = AUDIO_S16SYS;
  want.channels = 2;
  want.samples = 1024;
  want.callback = AudioCallback;
  want.userdata = this;

  SDL_AudioSpec got{};
  device_id_ = SDL_OpenAudioDevice(nullptr, 0, &want, &got, 0);

  if (device_id_ == 0) {
    SZ_LOG_ERROR("SDLAudioDevice: SDL_OpenAudioDevice failed: %s", SDL_GetError());
    return false;
  }

  out_obtained.sample_rate = got.freq;
  out_obtained.channels = got.channels;
  out_obtained.samples_per_callback = got.samples;

  SZ_LOG_INFO("SDLAudioDevice: Opened (freq=%d, channels=%d, samples=%d, format=0x%04X)",
              got.freq, got.channels, got.samples, got.format);

  if (got.freq != requested_sample_rate) {
    SZ_LOG_WARN("SDLAudioDevice: Obtained sample rate %d differs from requested %d!",
                got.freq, requested_sample_rate);
  }

  return true;
}

void SDLAudioDevice::Close() {
  if (device_id_ != 0) {
    SDL_CloseAudioDevice(device_id_);
    device_id_ = 0;
    SZ_LOG_INFO("SDLAudioDevice: Closed");
  }
  apu_ = nullptr;
}

void SDLAudioDevice::Start() {
  if (device_id_ != 0) {
    SDL_PauseAudioDevice(device_id_, 0);
    SZ_LOG_INFO("SDLAudioDevice: Started playback");
  }
}

void SDLAudioDevice::Stop() {
  if (device_id_ != 0) {
    SDL_PauseAudioDevice(device_id_, 1);
    SZ_LOG_INFO("SDLAudioDevice: Stopped playback");
  }
}

void SDLCALL SDLAudioDevice::AudioCallback(void* userdata, uint8_t* stream, int len) {
  auto* self = static_cast<SDLAudioDevice*>(userdata);
  auto* out = reinterpret_cast<int16_t*>(stream);
  int frames = len / (2 * static_cast<int>(sizeof(int16_t)));  // stereo int16

  if (self->apu_) {
    self->apu_->PopAudioFrames(out, frames);
  } else {
    SDL_memset(stream, 0, static_cast<size_t>(len));
  }
}

}  // namespace sz::app
