#ifndef SUPERZ80_DEVICES_INPUT_INPUTCONTROLLER_H
#define SUPERZ80_DEVICES_INPUT_INPUTCONTROLLER_H

namespace sz::input {

struct HostButtons {
  bool up = false;
  bool down = false;
  bool left = false;
  bool right = false;
  bool a = false;
  bool b = false;
  bool start = false;
  bool select = false;
};

struct DebugState {
  HostButtons buttons;
};

class InputController {
 public:
  void Reset();
  void SetHostButtons(const HostButtons& buttons);
  DebugState GetDebugState() const;

 private:
  HostButtons buttons_{};
};

}  // namespace sz::input

#endif
