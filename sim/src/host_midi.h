// OpenTracker simulator -- IMidiPort.
//  - Linux with ALSA (OT_SIM_ALSA): an ALSA sequencer client "OpenTracker"
//    with two virtual duplex ports, "OpenTracker DIN" and "OpenTracker USB";
//    connect them with aconnect or a DAW.
//  - Windows (WinMM): Windows has no virtual ports, so the simulator opens
//    existing devices chosen by name (--midi-in / --midi-out, a substring,
//    case-insensitive; --list-midi shows them). The device pair is the app's
//    USB MIDI port (a computer is a USB host on the device); the DIN port is
//    a null port. For a DAW on the same PC use a loopback driver such as
//    loopMIDI.
// Otherwise (or if nothing can be opened) it is a null port.
#pragma once

#include <cstdint>
#include <string>

#include "hal_api.h"

namespace sim {

class HostMidi final : public ot::IMidiPort {
public:
    HostMidi();
    ~HostMidi() override;
    // inName / outName: Windows device name substrings (ignored with ALSA).
    bool open(const std::string& inName = {}, const std::string& outName = {});   // false -> null port
    bool active() const { return impl_ != nullptr; }
    std::string describe() const;   // status line text
    static void listDevices();      // prints the host MIDI devices to stdout
    // --midi-log: every byte sent / received is printed to stderr.
    void setLog(bool on) { log_ = on; }
    bool send(ot::MidiPortId port, const uint8_t* data, uint32_t len) override;
    uint32_t receive(ot::MidiPortId port, uint8_t* dst, uint32_t cap) override;

private:
    bool sendRaw(ot::MidiPortId port, const uint8_t* data, uint32_t len);
    uint32_t receiveRaw(ot::MidiPortId port, uint8_t* dst, uint32_t cap);
    struct Impl;
    Impl* impl_ = nullptr;
    bool log_ = false;
};

}  // namespace sim
