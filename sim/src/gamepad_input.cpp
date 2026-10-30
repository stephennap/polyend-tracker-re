#include "gamepad_input.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include "host_hal.h"
#include "panel.h"
#include "ui/buttons.h"

namespace sim {

namespace {

const char* const kPhysNames[kPhysCount] = {"A",     "B",      "X",    "Y",  "L1",   "R1",   "L2",   "R2",
                                            "START", "SELECT", "MENU", "UP", "DOWN", "LEFT", "RIGHT"};

// Base layer: navigation and the most used keys. SELECT, L2 and R2 are
// modifiers; MENU opens the pad grid. SELECT+MENU quits.
const char kDefaultMap[] =
    "# OpenTracker handheld controls. One `[LAYER+]BUTTON = TARGET` per line.\n"
    "# BUTTON: A B X Y L1 R1 L2 R2 START SELECT MENU UP DOWN LEFT RIGHT\n"
    "# TARGET: a Tracker button (NOTE, INSTR, FX1, FX2, PATTERN, SONG, PLAY, REC,\n"
    "#   COPY, INSERT, DELETE, SHIFT, ENCPUSH, F1..F8, AUX, HELP, POWER,\n"
    "#   INSTRPARAMS, PERFORMANCE, FILE, CONFIG, MASTER, SAMPLELOADER,\n"
    "#   SAMPLEPLAYBACK, SAMPLEEDITOR, SAMPLERECORDER), ENC- / ENC+ (encoder),\n"
    "#   PADS (pad grid), QUIT, LAYER (modifier, base layer only) or NONE.\n"
    "# JOY n = BUTTON maps raw joystick button n (pads SDL does not recognise).\n"
    "\n"
    "UP = UP\nDOWN = DOWN\nLEFT = LEFT\nRIGHT = RIGHT\n"
    "A = ENCPUSH\nB = DELETE\nX = INSERT\nY = SHIFT\n"
    "L1 = ENC-\nR1 = ENC+\nSTART = PLAY\nMENU = PADS\n"
    "SELECT = LAYER\nL2 = LAYER\nR2 = LAYER\n"
    "\n"
    "# SELECT held: screens\n"
    "SELECT+UP = PATTERN\nSELECT+DOWN = SONG\nSELECT+LEFT = INSTRPARAMS\nSELECT+RIGHT = SAMPLELOADER\n"
    "SELECT+A = MASTER\nSELECT+B = FILE\nSELECT+X = CONFIG\nSELECT+Y = PERFORMANCE\n"
    "SELECT+L1 = SAMPLEEDITOR\nSELECT+R1 = SAMPLEPLAYBACK\nSELECT+START = SAMPLERECORDER\n"
    "SELECT+L2 = AUX\nSELECT+R2 = HELP\nSELECT+MENU = QUIT\n"
    "\n"
    "# L2 held: edit columns and record\n"
    "L2+A = NOTE\nL2+B = INSTR\nL2+X = FX1\nL2+Y = FX2\n"
    "L2+UP = COPY\nL2+START = REC\nL2+L1 = AUX\nL2+R1 = HELP\n"
    "L2+LEFT = LEFT\nL2+RIGHT = RIGHT\nL2+DOWN = DOWN\n"
    "\n"
    "# R2 held: screen buttons F1..F8 (D-pad = F1..F4, Y X A B = F5..F8)\n"
    "R2+LEFT = F1\nR2+UP = F2\nR2+RIGHT = F3\nR2+DOWN = F4\n"
    "R2+Y = F5\nR2+X = F6\nR2+A = F7\nR2+B = F8\n"
    "R2+START = POWER\nR2+L1 = ENC-\nR2+R1 = ENC+\n";

std::string upper(std::string s) {
    for (auto& c : s) c = char(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

int physByName(const std::string& n) {
    for (int i = 0; i < kPhysCount; ++i)
        if (n == kPhysNames[i]) return i;
    return -1;
}

const char* buttonLabel(uint8_t id) {
    int n = 0;
    const ButtonInfo* t = buttonTable(&n);
    for (int i = 0; i < n; ++i)
        if (t[i].id == id) return t[i].label;
    return "?";
}

}  // namespace

const char* physName(Phys p) { return int(p) < kPhysCount ? kPhysNames[int(p)] : "?"; }

const char* GamepadMap::defaultText() { return kDefaultMap; }

GamepadMap::GamepadMap() {
    for (auto& j : joy_) j = -1;
    std::string err;
    parse(kDefaultMap, &err);
}

bool GamepadMap::parse(const std::string& text, std::string* err) {
    std::istringstream is(text);
    std::string line;
    int lineNo = 0;
    bool ok = true;
    auto fail = [&](const std::string& msg) {
        if (err) *err += "line " + std::to_string(lineNo) + ": " + msg + "\n";
        ok = false;
    };
    while (std::getline(is, line)) {
        ++lineNo;
        const size_t hash = line.find('#');
        if (hash != std::string::npos) line.resize(hash);
        line = trim(line);
        if (line.empty()) continue;
        const size_t eq = line.find('=');
        if (eq == std::string::npos) { fail("missing '='"); continue; }
        const std::string lhs = upper(trim(line.substr(0, eq)));
        const std::string rhs = upper(trim(line.substr(eq + 1)));

        if (lhs.compare(0, 4, "JOY ") == 0) {
            const int n = std::atoi(lhs.c_str() + 4);
            const int p = physByName(rhs);
            if (n < 0 || n >= kJoyMax || p < 0) { fail("bad JOY line"); continue; }
            joy_[n] = int8_t(p);
            continue;
        }
        int layer = -1, phys;
        const size_t plus = lhs.find('+');
        if (plus != std::string::npos) {
            layer = physByName(trim(lhs.substr(0, plus)));
            phys = physByName(trim(lhs.substr(plus + 1)));
            if (layer < 0) { fail("unknown layer button"); continue; }
        } else {
            phys = physByName(lhs);
        }
        if (phys < 0) { fail("unknown button '" + lhs + "'"); continue; }

        PadAction a;
        if (rhs == "NONE") a.kind = PadAction::kNone;
        else if (rhs == "ENC-") a.kind = PadAction::kEncCcw;
        else if (rhs == "ENC+") a.kind = PadAction::kEncCw;
        else if (rhs == "PADS") a.kind = PadAction::kPads;
        else if (rhs == "QUIT") a.kind = PadAction::kQuit;
        else if (rhs == "LAYER") {
            if (layer >= 0) { fail("LAYER only in the base layer"); continue; }
            a.kind = PadAction::kLayer;
        } else {
            const int id = buttonByName(rhs.c_str());
            if (id < 0) { fail("unknown target '" + rhs + "'"); continue; }
            a.kind = PadAction::kButton;
            a.id = uint8_t(id);
        }
        map_[layer + 1][phys] = a;
    }
    return ok;
}

bool GamepadMap::loadFile(const std::string& path, std::string* err) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::stringstream ss;
    ss << f.rdbuf();
    return parse(ss.str(), err);
}

std::string GamepadMap::describeLayer(int layer) const {
    std::string out;
    for (int p = 0; p < kPhysCount; ++p) {
        const PadAction a = map_[layer + 1][p];
        const char* what = nullptr;
        switch (a.kind) {
            case PadAction::kButton: what = buttonLabel(a.id); break;
            case PadAction::kEncCcw: what = "Enc -"; break;
            case PadAction::kEncCw: what = "Enc +"; break;
            case PadAction::kPads: what = "Pads"; break;
            case PadAction::kQuit: what = "Quit"; break;
            default: break;
        }
        if (!what) continue;
        out += std::string(kPhysNames[p]) + " " + what + "\n";
    }
    return out;
}

// ---------------------------------------------------------------------------

HandheldInput::HandheldInput(SimInput& in, const GamepadMap& map) : in_(in), map_(map) {
    for (auto& v : pressed_) v = -1;
}

void HandheldInput::physical(Phys p, bool down, uint32_t nowMs) {
    const int pi = int(p);
    if (pi >= kPhysCount || down_[pi] == down) return;
    down_[pi] = down;

    // Pad grid: D-pad moves the cursor, A plays the pad, B closes the grid.
    // With a layer held the buttons keep their layer meaning.
    if (padMode_ && layer_ < 0 && pressed_[pi] < 0 &&
        (p == Phys::Up || p == Phys::Down || p == Phys::Left || p == Phys::Right || p == Phys::A ||
         p == Phys::B)) {
        if (p == Phys::A) {
            if (down) {
                heldPad_ = cursor_;
                in_.pad(uint8_t(cursor_), true, 100);
            } else if (heldPad_ >= 0) {
                in_.pad(uint8_t(heldPad_), false);
                heldPad_ = -1;
            }
            return;
        }
        if (!down) return;
        int col = cursor_ % ot::kPadCols, row = cursor_ / ot::kPadCols;
        switch (p) {
            case Phys::Up: row = (row + ot::kPadRows - 1) % ot::kPadRows; break;
            case Phys::Down: row = (row + 1) % ot::kPadRows; break;
            case Phys::Left: col = (col + ot::kPadCols - 1) % ot::kPadCols; break;
            case Phys::Right: col = (col + 1) % ot::kPadCols; break;
            default: run(PadAction{PadAction::kPads, 0}, p, true, nowMs); return;   // B closes
        }
        cursor_ = row * ot::kPadCols + col;
        return;
    }

    if (down) {
        const PadAction base = map_.at(-1, p);
        if (layer_ < 0 && base.kind == PadAction::kLayer) {
            layer_ = pi;
            return;
        }
        run(layer_ >= 0 ? map_.at(layer_, p) : base, p, true, nowMs);
        return;
    }
    if (pi == layer_) layer_ = -1;
    if (pressed_[pi] >= 0) {
        in_.button(uint8_t(pressed_[pi]), false);
        pressed_[pi] = -1;
    }
    if (encPhys_ == p) {
        encDir_ = 0;
        encPhys_ = Phys::Count;
    }
}

void HandheldInput::run(const PadAction& a, Phys p, bool down, uint32_t nowMs) {
    if (!down) return;
    switch (a.kind) {
        case PadAction::kButton:
            in_.button(a.id, true);
            pressed_[int(p)] = a.id;
            break;
        case PadAction::kEncCcw:
        case PadAction::kEncCw:
            encDir_ = a.kind == PadAction::kEncCw ? 1 : -1;
            encPhys_ = p;
            encNext_ = nowMs + 350;
            in_.encoder(encDir_);
            break;
        case PadAction::kPads:
            padMode_ = !padMode_;
            if (!padMode_ && heldPad_ >= 0) {
                in_.pad(uint8_t(heldPad_), false);
                heldPad_ = -1;
            }
            break;
        case PadAction::kQuit: quit_ = true; break;
        default: break;
    }
}

void HandheldInput::tick(uint32_t nowMs) {
    if (encDir_ && int32_t(nowMs - encNext_) >= 0) {
        in_.encoder(encDir_);
        encNext_ = nowMs + 60;
    }
}

}  // namespace sim
