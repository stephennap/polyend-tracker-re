#include "handheld.h"

#include <SDL.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>
#include <vector>

#include "canvas.h"
#include "gamepad_input.h"
#include "host_hal.h"
#include "sw_display.h"
#include "ui/buttons.h"

namespace sim {

namespace {

constexpr int kW = 800, kH = 480;
constexpr uint8_t kFont = 26;

// SDL game-controller buttons are positional (Xbox layout: A bottom, B right,
// X left, Y top); handheld labels are Nintendo-style (A right, B bottom).
int controllerToPhys(uint8_t b) {
    switch (b) {
        case SDL_CONTROLLER_BUTTON_A: return int(Phys::B);
        case SDL_CONTROLLER_BUTTON_B: return int(Phys::A);
        case SDL_CONTROLLER_BUTTON_X: return int(Phys::Y);
        case SDL_CONTROLLER_BUTTON_Y: return int(Phys::X);
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return int(Phys::L1);
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return int(Phys::R1);
        case SDL_CONTROLLER_BUTTON_START: return int(Phys::Start);
        case SDL_CONTROLLER_BUTTON_BACK: return int(Phys::Select);
        case SDL_CONTROLLER_BUTTON_GUIDE: return int(Phys::Menu);
        case SDL_CONTROLLER_BUTTON_DPAD_UP: return int(Phys::Up);
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return int(Phys::Down);
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return int(Phys::Left);
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return int(Phys::Right);
        default: return -1;
    }
}

// Keyboard stand-in for testing on a desktop (and for handhelds whose
// buttons arrive as keys).
int keyToPhys(SDL_Scancode sc) {
    switch (sc) {
        case SDL_SCANCODE_UP: return int(Phys::Up);
        case SDL_SCANCODE_DOWN: return int(Phys::Down);
        case SDL_SCANCODE_LEFT: return int(Phys::Left);
        case SDL_SCANCODE_RIGHT: return int(Phys::Right);
        case SDL_SCANCODE_X: return int(Phys::A);
        case SDL_SCANCODE_Z: return int(Phys::B);
        case SDL_SCANCODE_S: return int(Phys::X);
        case SDL_SCANCODE_A: return int(Phys::Y);
        case SDL_SCANCODE_Q: return int(Phys::L1);
        case SDL_SCANCODE_W: return int(Phys::R1);
        case SDL_SCANCODE_1: return int(Phys::L2);
        case SDL_SCANCODE_2: return int(Phys::R2);
        case SDL_SCANCODE_RETURN: return int(Phys::Start);
        case SDL_SCANCODE_RSHIFT:
        case SDL_SCANCODE_BACKSPACE: return int(Phys::Select);
        case SDL_SCANCODE_ESCAPE: return int(Phys::Menu);
        default: return -1;
    }
}

void drawPadGrid(Canvas& c, const SimLeds& leds, const SimInput& in, int cursor) {
    constexpr int kCellW = kW / ot::kPadCols, kCellH = 44, kTop = kH - ot::kPadRows * kCellH - 4;
    c.resetClip();
    c.setColor(0x000000, 200);
    c.fillRect(0, kTop - 26, kW, kH);
    c.setColor(0xE0E0E0);
    c.text(8, kTop - 24, kFont, "PADS   D-pad: move   A: play   B / MENU: close");
    for (int p = 0; p < ot::kPadCount; ++p) {
        const int x = (p % ot::kPadCols) * kCellW + 2, y = kTop + (p / ot::kPadCols) * kCellH + 2;
        if (p == cursor) {
            c.setColor(0xFFFFFF);
            c.fillRect(x - 2, y - 2, x + kCellW - 2, y + kCellH - 2);
        }
        uint32_t rgb = leds.color(p);
        if (in.padDown(uint8_t(p))) rgb = 0xF0F0F0;
        if (!rgb) rgb = 0x2A2A30;
        c.setColor(rgb);
        c.fillRect(x, y, x + kCellW - 6, y + kCellH - 6);
    }
}

void drawLayerHint(Canvas& c, const GamepadMap& map, int layer) {
    std::vector<std::string> lines;
    const std::string d = map.describeLayer(layer);
    size_t a = 0, e;
    while ((e = d.find('\n', a)) != std::string::npos) {
        lines.push_back(d.substr(a, e - a));
        a = e + 1;
    }
    const RomFont* f = romFont(kFont);
    const int lh = f->height + 2, cols = 3, colW = 250;
    const int rows = int(lines.size() + cols - 1) / cols;
    const int h = 30 + rows * lh + 8, y0 = kH - h;
    c.resetClip();
    c.setColor(0x000000, 210);
    c.fillRect(0, y0, kW, kH);
    c.setColor(0xF0C000);
    c.text(10, y0 + 4, kFont, (std::string(physName(Phys(layer))) + " held").c_str());
    c.setColor(0xE0E0E0);
    for (size_t i = 0; i < lines.size(); ++i)
        c.text(10 + int(i / size_t(rows)) * colW, y0 + 30 + int(i % size_t(rows)) * lh, kFont, lines[i].c_str());
}

}  // namespace

void drawHandheldOverlays(Canvas& c, const GamepadMap& map, const HandheldInput& pad, const SimLeds& leds,
                          const SimInput& in, bool startHint) {
    if (pad.padMode()) drawPadGrid(c, leds, in, pad.padCursor());
    if (pad.heldLayer() >= 0) drawLayerHint(c, map, pad.heldLayer());
    if (startHint && pad.heldLayer() < 0 && !pad.padMode()) {
        c.resetClip();
        c.setColor(0x000000, 200);
        c.fillRect(0, kH - 28, kW, kH);
        c.setColor(0xE0E0E0);
        c.text(8, kH - 26, kFont, "Hold SELECT / L2 / R2 to see more controls.  MENU: pads.  SELECT+MENU: quit");
    }
}

namespace {

void loadKeymap(GamepadMap& map, const std::string& path) {
    if (path.empty()) return;
    std::error_code ec;
    if (!std::filesystem::exists(std::filesystem::u8path(path), ec)) {
        std::ofstream out(std::filesystem::u8path(path), std::ios::binary);
        if (out) {
            out << GamepadMap::defaultText();
            std::printf("handheld: wrote the default controls to %s\n", path.c_str());
        }
        return;
    }
    std::string err;
    if (!map.loadFile(path, &err)) std::fprintf(stderr, "handheld: %s:\n%s", path.c_str(), err.c_str());
    else std::printf("handheld: controls from %s\n", path.c_str());
}

}  // namespace

int runHandheld(HandheldHost& h) {
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) != 0) {
        std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    // Extra controller mappings next to the control map (device bring-up).
    if (!h.keymapPath.empty()) {
        const std::string db =
            std::filesystem::u8path(h.keymapPath).parent_path().u8string() + "/gamecontrollerdb.txt";
        std::error_code ec;
        if (std::filesystem::exists(std::filesystem::u8path(db), ec))
            std::printf("handheld: %d mappings from %s\n", SDL_GameControllerAddMappingsFromFile(db.c_str()),
                        db.c_str());
    }
    GamepadMap map;
    loadKeymap(map, h.keymapPath);
    HandheldInput pad(h.input, map);

    const Uint32 flags = h.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : SDL_WINDOW_RESIZABLE;
    SDL_Window* win = SDL_CreateWindow("OpenTracker", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                       kW * h.scale, kH * h.scale, flags);
    if (!win) {
        std::fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        return 1;
    }
    if (h.fullscreen) SDL_ShowCursor(SDL_DISABLE);
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!ren) {
        std::fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
        return 1;
    }
    // 800x480 kept at its aspect ratio (letterboxed); smooth scaling, since
    // handheld panels are rarely an integer multiple.
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    SDL_RenderSetLogicalSize(ren, kW, kH);
    SDL_Texture* tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, kW, kH);
    {
        int ww = 0, wh = 0;
        SDL_GetRendererOutputSize(ren, &ww, &wh);
        std::printf("handheld: display %dx%d\n", ww, wh);
    }

    std::vector<SDL_GameController*> controllers;
    std::vector<SDL_Joystick*> joysticks;
    std::set<SDL_JoystickID> rawIds;   // joysticks SDL does not know as controllers
    auto openDevice = [&](int index) {
        if (SDL_IsGameController(index)) {
            if (SDL_GameController* gc = SDL_GameControllerOpen(index)) {
                controllers.push_back(gc);
                std::printf("handheld: controller \"%s\"\n", SDL_GameControllerName(gc));
            }
        } else if (SDL_Joystick* j = SDL_JoystickOpen(index)) {
            joysticks.push_back(j);
            rawIds.insert(SDL_JoystickInstanceID(j));
            std::printf("handheld: joystick \"%s\" (not a known controller; map it with JOY lines)\n",
                        SDL_JoystickName(j));
        }
    };

    Canvas canvas(kW, kH);
    std::vector<uint32_t> fb(kW * kH);
    bool quit = false;
    uint32_t lastPresent = 0;
    const uint32_t start = SDL_GetTicks();
    bool trig[2] = {false, false};
    int stick[2] = {0, 0};

    while (!quit) {
        const uint32_t now = SDL_GetTicks();
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
                case SDL_QUIT: quit = true; break;
                case SDL_JOYDEVICEADDED: openDevice(ev.jdevice.which); break;
                case SDL_CONTROLLERBUTTONDOWN:
                case SDL_CONTROLLERBUTTONUP: {
                    const int p = controllerToPhys(ev.cbutton.button);
                    if (p >= 0) pad.physical(Phys(p), ev.type == SDL_CONTROLLERBUTTONDOWN, now);
                    break;
                }
                case SDL_CONTROLLERAXISMOTION: {
                    const int v = ev.caxis.value;
                    if (ev.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT ||
                        ev.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT) {
                        const int t = ev.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT;
                        const bool on = trig[t] ? v > 8000 : v > 16000;
                        if (on != trig[t]) {
                            trig[t] = on;
                            pad.physical(t ? Phys::R2 : Phys::L2, on, now);
                        }
                    } else if (ev.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX ||
                               ev.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
                        // Left stick (if any) as a second D-pad.
                        const int a = ev.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY;
                        const int dir = v > 20000 ? 1 : v < -20000 ? -1 : (std::abs(v) < 12000 ? 0 : stick[a]);
                        if (dir != stick[a]) {
                            const Phys neg = a ? Phys::Up : Phys::Left, pos = a ? Phys::Down : Phys::Right;
                            if (stick[a]) pad.physical(stick[a] < 0 ? neg : pos, false, now);
                            if (dir) pad.physical(dir < 0 ? neg : pos, true, now);
                            stick[a] = dir;
                        }
                    }
                    break;
                }
                case SDL_JOYBUTTONDOWN:
                case SDL_JOYBUTTONUP: {
                    if (!rawIds.count(ev.jbutton.which)) break;
                    const bool down = ev.type == SDL_JOYBUTTONDOWN;
                    const int p = map.joy(ev.jbutton.button);
                    if (p >= 0) pad.physical(Phys(p), down, now);
                    else if (down) std::printf("handheld: joystick button %d (unmapped)\n", ev.jbutton.button);
                    break;
                }
                case SDL_JOYHATMOTION: {
                    if (!rawIds.count(ev.jhat.which)) break;
                    const uint8_t v = ev.jhat.value;
                    pad.physical(Phys::Up, v & SDL_HAT_UP, now);
                    pad.physical(Phys::Down, v & SDL_HAT_DOWN, now);
                    pad.physical(Phys::Left, v & SDL_HAT_LEFT, now);
                    pad.physical(Phys::Right, v & SDL_HAT_RIGHT, now);
                    break;
                }
                case SDL_KEYDOWN:
                case SDL_KEYUP: {
                    if (ev.key.repeat) break;
                    const bool down = ev.type == SDL_KEYDOWN;
                    if (down && ev.key.keysym.scancode == SDL_SCANCODE_Q && (ev.key.keysym.mod & KMOD_CTRL)) {
                        quit = true;
                        break;
                    }
                    const int p = keyToPhys(ev.key.keysym.scancode);
                    if (p >= 0) pad.physical(Phys(p), down, now);
                    else if (down)
                        std::printf("handheld: key %s (unmapped)\n", SDL_GetScancodeName(ev.key.keysym.scancode));
                    break;
                }
                default: break;
            }
        }
        pad.tick(now);
        if (pad.quitRequested()) quit = true;
        h.step();
        if (h.quitAfterMs >= 0 && long(now) >= h.quitAfterMs) quit = true;
        if (now - lastPresent >= 16) {
            lastPresent = now;
            h.display.frontCopy(fb.data());
            SwDisplay::dimFrame(fb.data(), fb.size(), h.display.backlight());
            std::memcpy(canvas.pixels(), fb.data(), fb.size() * 4);
            drawHandheldOverlays(canvas, map, pad, h.leds, h.input, now - start < 5000);
            SDL_UpdateTexture(tex, nullptr, canvas.pixels(), kW * 4);
            SDL_RenderClear(ren);
            SDL_RenderCopy(ren, tex, nullptr, nullptr);
            SDL_RenderPresent(ren);
        } else {
            SDL_Delay(1);
        }
    }
    for (auto* gc : controllers) SDL_GameControllerClose(gc);
    for (auto* j : joysticks) SDL_JoystickClose(j);
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    return 0;
}

}  // namespace sim
