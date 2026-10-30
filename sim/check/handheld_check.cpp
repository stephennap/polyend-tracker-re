// Handheld controls check: drives HandheldInput with physical buttons and
// checks the Tracker events that come out, parses a custom map, and saves
// the overlays (pad grid, a held layer) as PNGs for a look.
//   handheld-check OUT_PREFIX     -> OUT_PREFIX-pads.png, OUT_PREFIX-layer.png
#include <cstdio>
#include <string>
#include <vector>

#include "canvas.h"
#include "gamepad_input.h"
#include "handheld.h"
#include "host_hal.h"
#include "png.h"
#include "ui/buttons.h"

using namespace sim;
namespace b = ot::btn;
using Ev = ot::InputEvent;

namespace {

int failures = 0;

void expect(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

std::vector<Ev> drain(SimInput& in) {
    std::vector<Ev> out;
    Ev e;
    while (in.poll(e)) out.push_back(e);
    return out;
}

bool has(const std::vector<Ev>& v, Ev::Type t, int id) {
    for (const auto& e : v)
        if (e.type == t && (id < 0 || e.id == id)) return true;
    return false;
}

int encoderSum(const std::vector<Ev>& v) {
    int s = 0;
    for (const auto& e : v)
        if (e.type == Ev::kEncoder) s += e.delta;
    return s;
}

}  // namespace

int main(int argc, char** argv) {
    const std::string prefix = argc > 1 ? argv[1] : "handheld";
    SimClock clk;
    clk.setVirtual(true);
    SimInput in(clk);
    SimLeds leds;
    GamepadMap map;
    HandheldInput hi(in, map);
    uint32_t t = 0;
    auto press = [&](Phys p, bool down) { hi.physical(p, down, t); };

    // Base layer.
    press(Phys::A, true);
    expect(has(drain(in), Ev::kButtonDown, b::EncPush), "A presses ENCPUSH");
    press(Phys::A, false);
    expect(has(drain(in), Ev::kButtonUp, b::EncPush), "A release releases ENCPUSH");
    press(Phys::Start, true);
    press(Phys::Start, false);
    expect(has(drain(in), Ev::kButtonDown, b::Play), "START presses PLAY");

    // Layer: the release matches the press even when the layer is let go first.
    press(Phys::L2, true);
    expect(drain(in).empty(), "a layer button alone sends nothing");
    expect(hi.heldLayer() == int(Phys::L2), "L2 is the held layer");
    press(Phys::A, true);
    expect(has(drain(in), Ev::kButtonDown, b::Note), "L2+A presses NOTE");
    press(Phys::L2, false);
    press(Phys::A, false);
    expect(has(drain(in), Ev::kButtonUp, b::Note), "A release after L2 releases NOTE");

    press(Phys::R2, true);
    press(Phys::Y, true);
    expect(has(drain(in), Ev::kButtonDown, b::F5), "R2+Y presses F5");
    press(Phys::Y, false);
    press(Phys::R2, false);
    drain(in);

    press(Phys::Select, true);
    press(Phys::Up, true);
    expect(has(drain(in), Ev::kButtonDown, b::Pattern), "SELECT+UP presses PATTERN");
    press(Phys::Up, false);
    press(Phys::Select, false);
    drain(in);

    // Encoder with auto-repeat while held.
    press(Phys::R1, true);
    expect(encoderSum(drain(in)) == 1, "R1 turns the encoder +1");
    for (t = 0; t <= 1000; t += 10) hi.tick(t);
    const int reps = encoderSum(drain(in));
    expect(reps >= 8 && reps <= 14, "R1 held repeats about every 60 ms after 350 ms");
    press(Phys::R1, false);
    for (t = 1000; t <= 1500; t += 10) hi.tick(t);
    expect(encoderSum(drain(in)) == 0, "R1 release stops the repeat");

    // Pad grid.
    press(Phys::Menu, true);
    press(Phys::Menu, false);
    expect(hi.padMode(), "MENU opens the pad grid");
    for (Phys p : {Phys::Right, Phys::Right, Phys::Down}) {
        press(p, true);
        press(p, false);
    }
    expect(hi.padCursor() == 14, "D-pad moves the pad cursor");
    expect(drain(in).empty(), "moving the cursor sends nothing");
    press(Phys::A, true);
    expect(has(drain(in), Ev::kPadDown, 14), "A plays the pad under the cursor");
    leds.setPad(3, 0x20, 0x90, 0xff);
    leds.setPad(14, 0xff, 0x80, 0x10);
    leds.show();
    Canvas c(800, 480);
    c.resetClip();
    c.clearRect(0x404048);
    drawHandheldOverlays(c, map, hi, leds, in, false);
    expect(writePng((prefix + "-pads.png").c_str(), c.pixels(), 800, 480), "pads PNG written");
    press(Phys::A, false);
    expect(has(drain(in), Ev::kPadUp, 14), "A release releases the pad");
    press(Phys::B, true);
    press(Phys::B, false);
    expect(!hi.padMode(), "B closes the pad grid");
    drain(in);

    // Layer hint picture.
    press(Phys::Select, true);
    c.clearRect(0x404048);
    drawHandheldOverlays(c, map, hi, leds, in, false);
    expect(writePng((prefix + "-layer.png").c_str(), c.pixels(), 800, 480), "layer PNG written");
    // Quit combo.
    press(Phys::Menu, true);
    expect(hi.quitRequested(), "SELECT+MENU quits");

    // Custom map lines on top of the defaults; errors name the line.
    GamepadMap custom;
    std::string err;
    const bool ok = custom.parse("A = NOTE\nJOY 3 = START\nR2+B = enc+\nbogus line\nQ = PLAY\n", &err);
    expect(!ok && err.find("line 4") != std::string::npos && err.find("line 5") != std::string::npos,
           "bad lines are reported with their numbers");
    expect(custom.at(-1, Phys::A).kind == PadAction::kButton && custom.at(-1, Phys::A).id == b::Note,
           "A remapped to NOTE");
    expect(custom.at(-1, Phys::B).id == b::Delete, "unchanged lines keep the default");
    expect(custom.joy(3) == int(Phys::Start), "JOY 3 maps to START");
    expect(custom.at(int(Phys::R2), Phys::B).kind == PadAction::kEncCw, "targets are case-insensitive");

    if (failures) {
        std::fprintf(stderr, "handheld-check: %d failure(s)\n", failures);
        return 1;
    }
    std::printf("handheld-check: ok (%s-pads.png, %s-layer.png)\n", prefix.c_str(), prefix.c_str());
    return 0;
}
