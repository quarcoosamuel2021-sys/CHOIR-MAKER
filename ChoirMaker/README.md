# ChoirMaker

One vocal in, a whole choir out. A VST3 / AU / Standalone effect plugin (built with JUCE) that works in any DAW:
Ableton Live, FL Studio, Logic, Cubase, Reaper, Studio One, Bitwig, Pro Tools (via VST3 hosts), GarageBand (AU) and more.

## How it makes a choir from one voice

Each of up to 16 virtual singers gets:

- **Its own pitch** – small static detune plus slow random drift, so singers never sit exactly on the same note.
- **Its own vibrato** – different rate, depth and phase per singer.
- **Its own timing** – each singer enters a few milliseconds apart, like people in a room.
- **Its own formants** – a 3-band vowel filter (A–E–I–O–U morph) with every singer a little different, so it sounds like many throats and not one voice copied.
- **Its own place in the stereo field** – spread from centre outward.
- Optional **harmony** – singers can sing a chord (octaves, open 5th, major, minor, sus, 7ths) built on the note you sing.

Then a low cut, an "Air" low-pass, and a reverb give the hall.

The pitch shifter is a time-domain SOLA design: the read head moves at the pitch ratio and, when it runs out of room, jumps to the spot that best lines up with the waveform (cross-correlation), with a short crossfade. That keeps sustained notes free of the phasey/tremolo artefacts of a simple two-tap shifter.

## Controls

| Control | What it does |
|---|---|
| Voices | Number of singers (1–16) |
| Harmony | Unison ensemble, octaves, open 5th, major/minor, sus, 7ths, "voice on top" chords |
| Vowel / Formant | Which vowel the choir sings and how strongly the vowel shaping is applied |
| Detune | Spread of per-singer tuning (cents) |
| Timing | Max per-singer entry offset (ms) |
| Vib Depth / Vib Rate | Vibrato per singer |
| Width | Stereo spread |
| Room / Reverb | Reverb size and amount |
| Air | Brightness of the choir |
| Mix / Output | Dry↔choir balance, output trim |

**Tips**
- Start with *Unison*, 8 voices, Mix ≈ 60 % for an "ensemble of the same singer" sound. Raise Detune/Timing for a bigger, looser crowd.
- For pads, pick *Major* or *Minor* and **sing the root note** – every note becomes a chord.
- Feed it a clean, dry, mono vocal. Compress lightly before it, not after.
- The plugin reports its latency to the DAW (about 30 ms) and delays the dry signal to match, so dry and choir stay aligned.

## Build

Requirements: CMake ≥ 3.22, a C++17 compiler. JUCE is downloaded automatically.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Plugins appear under `build/ChoirMaker_artefacts/Release/` (`VST3/`, `AU/` on macOS, `Standalone/`).
Copy the `.vst3` to your VST3 folder (Windows: `C:\Program Files\Common Files\VST3`, macOS: `~/Library/Audio/Plug-Ins/VST3`, Linux: `~/.vst3`), then rescan in your DAW.

**Linux build dependencies:** `libasound2-dev libfreetype-dev libfontconfig1-dev libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev libxrender-dev libglu1-mesa-dev mesa-common-dev`

### No compiler? Let GitHub build it

Push this folder to a GitHub repository. The workflow in `.github/workflows/build.yml` builds Windows, macOS (universal arm64 + Intel) and Linux and attaches the plugins as downloadable artifacts under the **Actions** tab.

### macOS note
Unsigned plugins are quarantined by Gatekeeper. After copying, run  
`xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/ChoirMaker.vst3`  
(same for the `.component`). To distribute publicly you will need an Apple Developer ID to sign and notarize.

## DSP test

The DSP core (`Source/ChoirEngine.h`) has no JUCE dependency and is tested stand-alone:

```bash
g++ -std=c++17 -O2 -ISource tests/engine_test.cpp -o engine_test && ./engine_test
```

It checks pitch accuracy of the shifter, output level and stability across all chords and extreme settings, and click-free live parameter changes.

## Known limits / ideas for v2

- Harmony intervals are relative to the note you sing (the plugin does not detect pitch or key). A scale-locked mode with a pitch tracker would let you sing a melody and have the choir follow a chosen key.
- The pitch shifter is tuned for voices; very noisy or percussive material will sound grainy.
- CLAP and LV2 builds are not enabled by default (`-DCHOIR_BUILD_LV2=ON` for LV2 on Linux).
