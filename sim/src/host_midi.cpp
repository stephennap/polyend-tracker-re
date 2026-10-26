#include "host_midi.h"

#include <cstdio>

#ifdef OT_SIM_ALSA
#include <alsa/asoundlib.h>
#elif defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>

#include <atomic>
#include <mutex>
#include <vector>

#include "portable.h"
#endif

namespace sim {

#ifdef OT_SIM_ALSA
// ---------------------------------------------------------------------------
// Linux: ALSA sequencer, two virtual duplex ports.
struct HostMidi::Impl {
    snd_seq_t* seq = nullptr;
    int port[2] = {-1, -1};
    snd_midi_event_t* enc[2] = {};
    snd_midi_event_t* dec = nullptr;
    static constexpr uint32_t kRx = 4096;
    uint8_t rx[2][kRx] = {};
    uint32_t rxHead[2] = {}, rxTail[2] = {};

    void pump() {
        snd_seq_event_t* ev = nullptr;
        while (snd_seq_event_input_pending(seq, 1) > 0 && snd_seq_event_input(seq, &ev) >= 0 && ev) {
            const int p = ev->dest.port == port[1] ? 1 : 0;
            uint8_t buf[256];
            snd_midi_event_reset_decode(dec);
            const long n = snd_midi_event_decode(dec, buf, sizeof(buf), ev);
            for (long i = 0; i < n; ++i) {
                if (rxHead[p] - rxTail[p] >= kRx) break;
                rx[p][rxHead[p]++ % kRx] = buf[i];
            }
            ev = nullptr;
        }
    }
};

HostMidi::HostMidi() = default;

HostMidi::~HostMidi() {
    if (impl_) {
        for (auto* e : impl_->enc)
            if (e) snd_midi_event_free(e);
        if (impl_->dec) snd_midi_event_free(impl_->dec);
        snd_seq_close(impl_->seq);
        delete impl_;
    }
}

bool HostMidi::open(const std::string&, const std::string&) {
    snd_seq_t* seq = nullptr;
    if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, SND_SEQ_NONBLOCK) < 0) {
        std::fprintf(stderr, "sim midi: ALSA sequencer unavailable, MIDI disabled\n");
        return false;
    }
    snd_seq_set_client_name(seq, "OpenTracker");
    auto* im = new Impl;
    im->seq = seq;
    const char* names[2] = {"OpenTracker DIN", "OpenTracker USB"};
    const unsigned caps = SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ | SND_SEQ_PORT_CAP_WRITE |
                          SND_SEQ_PORT_CAP_SUBS_WRITE | SND_SEQ_PORT_CAP_DUPLEX;
    for (int i = 0; i < 2; ++i) {
        im->port[i] = snd_seq_create_simple_port(seq, names[i], caps,
                                                 SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
        snd_midi_event_new(256, &im->enc[i]);
    }
    snd_midi_event_new(256, &im->dec);
    snd_midi_event_no_status(im->dec, 1);   // always emit full status bytes
    impl_ = im;
    return true;
}

std::string HostMidi::describe() const { return impl_ ? "ALSA ports OpenTracker DIN/USB" : "off"; }

void HostMidi::listDevices() {
    std::printf("ALSA: the simulator creates its own ports \"OpenTracker DIN\" / \"OpenTracker USB\";\n"
                "connect them with aconnect (aconnect -l lists the system's ports).\n");
}

bool HostMidi::send(ot::MidiPortId port, const uint8_t* data, uint32_t len) {
    if (!impl_) return true;   // swallowed
    const int p = port == ot::MidiPortId::kUsb ? 1 : 0;
    for (uint32_t i = 0; i < len; ++i) {
        snd_seq_event_t ev;
        snd_seq_ev_clear(&ev);
        if (snd_midi_event_encode_byte(impl_->enc[p], data[i], &ev) == 1) {
            snd_seq_ev_set_source(&ev, impl_->port[p]);
            snd_seq_ev_set_subs(&ev);
            snd_seq_ev_set_direct(&ev);
            snd_seq_event_output(impl_->seq, &ev);
        }
    }
    snd_seq_drain_output(impl_->seq);
    return true;
}

uint32_t HostMidi::receive(ot::MidiPortId port, uint8_t* dst, uint32_t cap) {
    if (!impl_) return 0;
    impl_->pump();
    const int p = port == ot::MidiPortId::kUsb ? 1 : 0;
    uint32_t n = 0;
    while (n < cap && impl_->rxTail[p] != impl_->rxHead[p]) dst[n++] = impl_->rx[p][impl_->rxTail[p]++ % Impl::kRx];
    return n;
}

#elif defined(_WIN32)
// ---------------------------------------------------------------------------
// Windows: WinMM devices chosen by name, used as the app's USB MIDI port.
namespace {

std::string narrow(const wchar_t* w) {
    const int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 1) return {};
    std::string s(size_t(n - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, -1, &s[0], n, nullptr, nullptr);
    return s;
}

bool containsNoCase(const std::string& hay, const std::string& needle) {
    if (needle.empty()) return false;
    for (size_t i = 0; i + needle.size() <= hay.size(); ++i)
        if (strcaseCmp(hay.substr(i, needle.size()).c_str(), needle.c_str()) == 0) return true;
    return false;
}

std::vector<std::string> inNames() {
    std::vector<std::string> v;
    const UINT n = midiInGetNumDevs();
    for (UINT i = 0; i < n; ++i) {
        MIDIINCAPSW c{};
        v.push_back(midiInGetDevCapsW(i, &c, sizeof c) == MMSYSERR_NOERROR ? narrow(c.szPname) : std::string());
    }
    return v;
}

std::vector<std::string> outNames() {
    std::vector<std::string> v;
    const UINT n = midiOutGetNumDevs();
    for (UINT i = 0; i < n; ++i) {
        MIDIOUTCAPSW c{};
        v.push_back(midiOutGetDevCapsW(i, &c, sizeof c) == MMSYSERR_NOERROR ? narrow(c.szPname) : std::string());
    }
    return v;
}

int findDevice(const std::vector<std::string>& names, const std::string& want) {
    for (size_t i = 0; i < names.size(); ++i)
        if (strcaseCmp(names[i].c_str(), want.c_str()) == 0) return int(i);   // exact name first
    for (size_t i = 0; i < names.size(); ++i)
        if (containsNoCase(names[i], want)) return int(i);
    return -1;
}

// Number of data bytes after a status byte (sysex excluded).
int dataBytes(uint8_t status) {
    if (status < 0xF0) return (status & 0xE0) == 0xC0 ? 1 : 2;   // Cx / Dx: 1, others: 2
    switch (status) {
        case 0xF1: case 0xF3: return 1;
        case 0xF2: return 2;
        default: return 0;   // F6, F8..FF
    }
}

}  // namespace

struct HostMidi::Impl {
    HMIDIIN in = nullptr;
    HMIDIOUT out = nullptr;
    std::string inName, outName;

    // Input ring (filled on the WinMM callback thread).
    static constexpr uint32_t kRx = 4096;
    std::mutex mu;
    uint8_t rx[kRx] = {};
    uint32_t rxHead = 0, rxTail = 0;
    std::atomic<bool> closing{false};

    // Sysex input buffers; refilled from receive() (WinMM functions must not
    // be called from inside the callback).
    static constexpr int kSysexBufs = 4;
    static constexpr DWORD kSysexLen = 1024;
    MIDIHDR inHdr[kSysexBufs] = {};
    char inBuf[kSysexBufs][kSysexLen] = {};
    std::atomic<bool> requeue[kSysexBufs] = {};

    // Output parser: the app writes a raw byte stream.
    uint8_t status = 0;    // current (running) status
    uint8_t msg[3] = {};
    int have = 0, need = 0;
    std::vector<uint8_t> sysex;
    bool inSysex = false;

    void push(const uint8_t* b, size_t n) {
        std::lock_guard<std::mutex> lk(mu);
        for (size_t i = 0; i < n; ++i) {
            if (rxHead - rxTail >= kRx) break;   // full: drop, like an overrun
            rx[rxHead++ % kRx] = b[i];
        }
    }

    static void CALLBACK inProc(HMIDIIN, UINT wMsg, DWORD_PTR inst, DWORD_PTR p1, DWORD_PTR) {
        auto* self = reinterpret_cast<Impl*>(inst);
        if (wMsg == MIM_DATA) {
            const uint8_t b[3] = {uint8_t(p1), uint8_t(p1 >> 8), uint8_t(p1 >> 16)};
            if (b[0] < 0x80) return;   // no status (should not happen)
            self->push(b, size_t(1 + dataBytes(b[0])));
        } else if (wMsg == MIM_LONGDATA) {
            auto* h = reinterpret_cast<MIDIHDR*>(p1);
            if (self->closing) return;
            if (h->dwBytesRecorded) self->push(reinterpret_cast<const uint8_t*>(h->lpData), h->dwBytesRecorded);
            self->requeue[h->dwUser] = true;
        }
    }

    void requeueSysex() {
        for (int i = 0; i < kSysexBufs; ++i)
            if (requeue[i].exchange(false)) midiInAddBuffer(in, &inHdr[i], sizeof(MIDIHDR));
    }

    bool openIn(UINT id) {
        if (midiInOpen(&in, id, reinterpret_cast<DWORD_PTR>(&inProc), reinterpret_cast<DWORD_PTR>(this),
                       CALLBACK_FUNCTION) != MMSYSERR_NOERROR) {
            in = nullptr;
            return false;
        }
        for (int i = 0; i < kSysexBufs; ++i) {
            inHdr[i].lpData = inBuf[i];
            inHdr[i].dwBufferLength = kSysexLen;
            inHdr[i].dwUser = DWORD_PTR(i);
            midiInPrepareHeader(in, &inHdr[i], sizeof(MIDIHDR));
            midiInAddBuffer(in, &inHdr[i], sizeof(MIDIHDR));
        }
        midiInStart(in);
        return true;
    }

    void sendShort() {
        const DWORD m = DWORD(msg[0]) | DWORD(msg[1]) << 8 | DWORD(msg[2]) << 16;
        midiOutShortMsg(out, m);
    }

    void sendSysex() {
        MIDIHDR h{};
        h.lpData = reinterpret_cast<LPSTR>(sysex.data());
        h.dwBufferLength = DWORD(sysex.size());
        if (midiOutPrepareHeader(out, &h, sizeof h) != MMSYSERR_NOERROR) return;
        if (midiOutLongMsg(out, &h, sizeof h) == MMSYSERR_NOERROR)
            for (int i = 0; i < 1000 && !(h.dwFlags & MHDR_DONE); ++i) Sleep(1);   // rare; wait for the driver
        midiOutUnprepareHeader(out, &h, sizeof h);
    }

    void outByte(uint8_t b) {
        if (b >= 0xF8) {   // real-time: may appear anywhere, even inside sysex
            msg[0] = b;
            msg[1] = msg[2] = 0;
            sendShort();
            return;
        }
        if (b == 0xF0) {
            inSysex = true;
            sysex.assign(1, b);
            status = 0;
            return;
        }
        if (inSysex) {
            if (b < 0x80) {
                if (sysex.size() < 65536) sysex.push_back(b);
                return;
            }
            sysex.push_back(0xF7);   // F7 or any status byte ends it
            inSysex = false;
            sendSysex();
            if (b == 0xF7) return;
        }
        if (b >= 0x80) {
            status = b;
            msg[0] = b;
            have = 0;
            need = dataBytes(b);
            if (need == 0) {
                msg[1] = msg[2] = 0;
                sendShort();
                status = 0;   // system common: no running status
            }
            return;
        }
        if (!status) return;   // data byte without status: dropped
        msg[1 + have++] = b;
        if (have == need) {
            if (need == 1) msg[2] = 0;
            sendShort();
            have = 0;
            if (status >= 0xF0) status = 0;
            msg[0] = status;   // running status for channel messages
        }
    }

    ~Impl() {
        closing = true;
        if (in) {
            midiInStop(in);
            midiInReset(in);   // returns the sysex buffers
            for (int i = 0; i < kSysexBufs; ++i) midiInUnprepareHeader(in, &inHdr[i], sizeof(MIDIHDR));
            midiInClose(in);
        }
        if (out) {
            midiOutReset(out);
            midiOutClose(out);
        }
    }
};

HostMidi::HostMidi() = default;
HostMidi::~HostMidi() { delete impl_; }

bool HostMidi::open(const std::string& inName, const std::string& outName) {
    if (inName.empty() && outName.empty()) {
        std::fprintf(stderr, "sim midi: no device chosen, MIDI off (--list-midi, then --midi-in / --midi-out NAME)\n");
        return false;
    }
    auto* im = new Impl;
    if (!inName.empty()) {
        const auto names = inNames();
        const int id = findDevice(names, inName);
        if (id < 0)
            std::fprintf(stderr, "sim midi: no MIDI input matches \"%s\"\n", inName.c_str());
        else if (!im->openIn(UINT(id)))
            std::fprintf(stderr, "sim midi: cannot open MIDI input \"%s\" (in use?)\n", names[size_t(id)].c_str());
        else
            im->inName = names[size_t(id)];
    }
    if (!outName.empty()) {
        const auto names = outNames();
        const int id = findDevice(names, outName);
        if (id < 0)
            std::fprintf(stderr, "sim midi: no MIDI output matches \"%s\"\n", outName.c_str());
        else if (midiOutOpen(&im->out, UINT(id), 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
            im->out = nullptr;
            std::fprintf(stderr, "sim midi: cannot open MIDI output \"%s\" (in use?)\n", names[size_t(id)].c_str());
        } else {
            im->outName = names[size_t(id)];
        }
    }
    if (!im->in && !im->out) {
        delete im;
        return false;
    }
    std::fprintf(stderr, "sim midi: USB port in \"%s\", out \"%s\"\n", im->in ? im->inName.c_str() : "-",
                 im->out ? im->outName.c_str() : "-");
    impl_ = im;
    return true;
}

std::string HostMidi::describe() const {
    if (!impl_) return "off";
    return "USB in: " + (impl_->in ? impl_->inName : std::string("-")) +
           ", out: " + (impl_->out ? impl_->outName : std::string("-"));
}

void HostMidi::listDevices() {
    const auto ins = inNames(), outs = outNames();
    std::printf("MIDI inputs (--midi-in NAME):\n");
    if (ins.empty()) std::printf("  (none)\n");
    for (size_t i = 0; i < ins.size(); ++i) std::printf("  %zu: %s\n", i, ins[i].c_str());
    std::printf("MIDI outputs (--midi-out NAME):\n");
    if (outs.empty()) std::printf("  (none)\n");
    for (size_t i = 0; i < outs.size(); ++i) std::printf("  %zu: %s\n", i, outs[i].c_str());
}

bool HostMidi::send(ot::MidiPortId port, const uint8_t* data, uint32_t len) {
    if (!impl_ || !impl_->out || port != ot::MidiPortId::kUsb) return true;   // swallowed
    for (uint32_t i = 0; i < len; ++i) impl_->outByte(data[i]);
    return true;
}

uint32_t HostMidi::receive(ot::MidiPortId port, uint8_t* dst, uint32_t cap) {
    if (!impl_ || !impl_->in || port != ot::MidiPortId::kUsb) return 0;
    impl_->requeueSysex();
    std::lock_guard<std::mutex> lk(impl_->mu);
    uint32_t n = 0;
    while (n < cap && impl_->rxTail != impl_->rxHead) dst[n++] = impl_->rx[impl_->rxTail++ % Impl::kRx];
    return n;
}

#else
// ---------------------------------------------------------------------------
// No MIDI backend: null port.
struct HostMidi::Impl {};

HostMidi::HostMidi() = default;
HostMidi::~HostMidi() = default;

bool HostMidi::open(const std::string&, const std::string&) { return false; }
std::string HostMidi::describe() const { return "off"; }
void HostMidi::listDevices() { std::printf("this build has no MIDI backend\n"); }

bool HostMidi::send(ot::MidiPortId, const uint8_t*, uint32_t) { return true; }
uint32_t HostMidi::receive(ot::MidiPortId, uint8_t*, uint32_t) { return 0; }
#endif

}  // namespace sim
