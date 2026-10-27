OpenTracker simulator for Windows
=================================

A desktop simulator of OpenTracker, the open-source firmware for the original
Polyend Tracker. It runs the same code as the firmware, in a window, with
sound. Nothing to install: unzip and run.

Not affiliated with or endorsed by Polyend.

Start
-----
Double-click OpenTracker.exe. It uses the "sd-card" folder next to it as the
Tracker's SD card, which already holds the OpenTracker test songs
(File > Open Project). The workspace is saved to the card when you close
the window.

OpenTracker.exe opens no console window. Its messages go to
sd-card.log next to it; look there if something does not work.

opentracker-sim.exe is the same program for a terminal (Command Prompt or
PowerShell): it prints its messages there and takes the options below.
Both use the "sd-card" folder when it is next to them; without it, the card
is %USERPROFILE%\.opentracker-sd (created on first run).

Keys
----
F1..F8           screen buttons under the display
F9 / F10         Pattern / Song
Arrows           Up / Down / Left / Right
Space            Play
Shift            Shift
Enter            encoder push
Page Up / Down   turn the encoder (or use the mouse wheel)
Tab              switch between COMMAND mode and PAD mode
                 (in PAD mode the letter and digit rows play the pads)
1..5 (COMMAND)   Instrument Params, Performance, File, Config, Master
6..0 (COMMAND)   Sample Loader, Playback, Editor, Recorder, Song
Ctrl+Q           quit

Click any button on the panel with the mouse; right-click latches it
(for example Shift). The full key map is in sim/README.md in the source.

MIDI
----
List the devices:
    opentracker-sim.exe --list-midi
Use one (the name can be part of the device name):
    opentracker-sim.exe --midi-in "KeyStep" --midi-out "KeyStep"

To start OpenTracker.exe with MIDI from a double-click, make a shortcut to
it and add the options to the shortcut's Target, after the path.

Then switch the Tracker's MIDI on in Config > MIDI (Notes in, MIDI out,
Clock out ...: USB). They are all off by default.

The device acts as the Tracker's USB MIDI port. A device can be open in
only one program at a time. To connect to a DAW on the same PC, use a
loopback driver such as loopMIDI.

More options
------------
    opentracker-sim.exe --help

Source: https://github.com/stephennap/polyend-tracker-re (branch windows-port)
Third-party licences are in the "licenses" folder.
