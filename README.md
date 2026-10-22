# MixMate

Headphone correction for the end of your mix or master chain.

- **Thousands of headphones**: correction curves from [AutoEQ](https://github.com/jaakkopasanen/AutoEq), with the 20 most popular studio models at the top and a search box for the rest.
- **Two headphones, one key**: press **N** to cycle Off (speakers) → Headphone 1 → Headphone 2.
- **Mid / Side**: press **M** to cycle Stereo → Mid → Side.
- **Band solo**: press **B** to cycle Off → Sub → Bass → Mids → Highs (crossovers adjustable, default 100 Hz / 250 Hz / 4 kHz).
- **Strength** knob, automatic level matching, zero latency, and automatic bypass while your DAW exports.
- **Set once and forget**: settings save with your project, and *Save as default* makes every new MixMate start your way.

## Download

Get the latest build from the [Releases page](https://github.com/gillyvst/MixMate/releases/latest):

- **Mac** (AU + VST3, Apple Silicon and Intel): `MixMate-mac.zip`, then follow `README.txt` inside.
- **Windows** (VST3): `MixMate-windows.zip`, then copy `MixMate.vst3` into `C:\Program Files\Common Files\VST3`.

## Shortcut keys with the window closed

The keys work while the MixMate window is focused. To switch with the window closed, map the
**Cycle Headphones**, **Cycle Monitor** and **Cycle Solo** controls to a key or MIDI button in your DAW
(Ableton: Cmd/Ctrl+K, click the control, press a key). Every press moves to the next option.

## Building

Every push to `main` is built automatically by GitHub Actions and published as a release.
To build locally you need CMake 3.22+ and a C++17 compiler:

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

`-DMIXMATE_TESTS=ON` adds a test program that checks the audio processing against the AutoEQ curves.

The headphone list (`Resources/headphones.tsv`) is generated from AutoEQ's results with
`tools/build_headphone_db.py`; see the script for how to refresh it.

## Credits

Headphone data: AutoEQ by Jaakko Pasanen (MIT). Font: Poppins (OFL). Built with [JUCE](https://juce.com).
See `packaging/THIRD-PARTY-NOTICES.txt`.
