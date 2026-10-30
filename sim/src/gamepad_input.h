// OpenTracker simulator -- handheld controls (TrimUI Brick and similar).
//
// A handheld has a D-pad, A/B/X/Y, L1/R1/L2/R2, Start, Select and Menu. The
// Tracker has F1..F8, the 5x5 key block, Aux/Help, the encoder and 48 pads.
// Buttons marked LAYER in the base layer are modifiers: while one is held,
// the other buttons use that layer's actions. MENU opens an on-screen pad
// grid (D-pad picks a pad, A plays it).
//
// Button names are the labels printed on a Nintendo-style handheld (A right,
// B bottom, X top, Y left); SDL's game-controller buttons are positional
// (Xbox layout), and are converted.
//
// The map is plain text, one `[LAYER+]BUTTON = TARGET` per line, with TARGET
// a Tracker button name (panel.cpp table: NOTE, F1, SHIFT, ...), ENC-, ENC+,
// PADS, QUIT, LAYER or NONE. `JOY n = BUTTON` maps raw joystick button n for
// pads SDL does not know as game controllers. The built-in map is the default;
// a file only needs the lines it changes.
#pragma once

#include <cstdint>
#include <string>

namespace sim {

class SimInput;

enum class Phys : uint8_t { A, B, X, Y, L1, R1, L2, R2, Start, Select, Menu, Up, Down, Left, Right, Count };
constexpr int kPhysCount = int(Phys::Count);
const char* physName(Phys p);

struct PadAction {
    enum Kind : uint8_t { kNone, kButton, kEncCcw, kEncCw, kPads, kQuit, kLayer } kind = kNone;
    uint8_t id = 0;   // Tracker button id for kButton
};

class GamepadMap {
public:
    GamepadMap();   // built-in defaults
    // Applies the lines of a map (file contents); errors name the line.
    bool parse(const std::string& text, std::string* err);
    bool loadFile(const std::string& path, std::string* err);
    // layer: -1 = base, otherwise the held layer button.
    PadAction at(int layer, Phys p) const { return map_[layer + 1][int(p)]; }
    // Raw joystick button -> Phys, or -1.
    int joy(int button) const { return button >= 0 && button < kJoyMax ? joy_[button] : -1; }
    // One line per mapped button of a layer ("A Note  B Instr ..."), for the hint overlay.
    std::string describeLayer(int layer) const;
    static const char* defaultText();

private:
    static constexpr int kJoyMax = 32;
    PadAction map_[kPhysCount + 1][kPhysCount];
    int8_t joy_[kJoyMax];
};

// Turns physical button presses into SimInput events.
class HandheldInput {
public:
    HandheldInput(SimInput& in, const GamepadMap& map);
    void physical(Phys p, bool down, uint32_t nowMs);
    void tick(uint32_t nowMs);   // encoder auto-repeat
    bool quitRequested() const { return quit_; }
    bool padMode() const { return padMode_; }
    int padCursor() const { return cursor_; }
    int heldLayer() const { return layer_; }

private:
    void run(const PadAction& a, Phys p, bool down, uint32_t nowMs);
    SimInput& in_;
    const GamepadMap& map_;
    int layer_ = -1;              // held layer button, -1 none
    int16_t pressed_[kPhysCount]; // Tracker button id pressed by each Phys, -1 none
    bool down_[kPhysCount] = {};
    bool padMode_ = false, quit_ = false;
    int cursor_ = 0, heldPad_ = -1;
    int encDir_ = 0;              // held ENC-/ENC+ direction
    Phys encPhys_ = Phys::Count;
    uint32_t encNext_ = 0;
};

}  // namespace sim
