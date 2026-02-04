#ifndef SUPERZ80_APP_INPUTHOST_H
#define SUPERZ80_APP_INPUTHOST_H

#include "devices/input/InputController.h"

namespace sz::app {

class InputHost {
 public:
  sz::input::HostButtons ReadButtons() const;
};

}  // namespace sz::app

#endif
