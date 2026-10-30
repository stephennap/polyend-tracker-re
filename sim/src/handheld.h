// OpenTracker simulator -- handheld mode (TrimUI Brick and similar Linux
// handhelds): the 800x480 screen scaled to the display, gamepad controls
// (gamepad_input.h), the pad grid and a hint box drawn over the screen.
#pragma once

#include <functional>
#include <string>

namespace sim {

class Canvas;
class GamepadMap;
class HandheldInput;
class SimInput;
class SimLeds;
class SwDisplay;

struct HandheldHost {
    SimInput& input;
    SwDisplay& display;
    const SimLeds& leds;
    std::function<void()> step;   // runs the app (Machine::step)
    std::string keymapPath;       // control map; written with the defaults when missing
    bool fullscreen = false;
    int scale = 1;                // window scale when not fullscreen
    long quitAfterMs = -1;
};

int runHandheld(HandheldHost& h);

// Pad grid, held-layer hint and the start hint over the 800x480 screen.
void drawHandheldOverlays(Canvas& c, const GamepadMap& map, const HandheldInput& pad, const SimLeds& leds,
                          const SimInput& in, bool startHint);

}  // namespace sim
