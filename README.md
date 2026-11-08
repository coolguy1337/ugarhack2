# Pitch Hack – VST3 / CLAP

A VST3 port of the Max for Live device **Pitch Hack.amxd**, rebuilt in C++/JUCE from the
device's own patch graph (including the embedded `vs.*`, `pluggo.dryWet2~` and `vs.limiter~`
abstractions). Stereo in / stereo out, no latency reported (like the original).

## Build

Needs CMake ≥ 3.22 and a C++17 compiler. JUCE 8.0.8 is downloaded automatically.

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release --target PitchHack_VST3 PitchHack_CLAP

Output: `build/PitchHack_artefacts/Release/VST3/Pitch Hack.vst3` and `.../CLAP/Pitch Hack.clap`
(CLAP support comes from clap-juce-extensions, fetched automatically).

* **Windows** – copy the `.vst3` to `C:\Program Files\Common Files\VST3\` and the `.clap` to
  `C:\Program Files\Common Files\CLAP\` (static CRT, no redistributable).
* **macOS** – universal (arm64 + x86_64) build; copy to `~/Library/Audio/Plug-Ins/VST3/`. The binary is
  unsigned, so run `xattr -cr "Pitch Hack.vst3"` once.
* **Linux** – copy to `~/.vst3/`.
* **No local toolchain?** Push this folder to GitHub: `.github/workflows/build.yml` builds all three
  platforms and attaches the plug-ins as downloadable artifacts.

## Parameters (names, ranges and defaults taken from the .amxd)

| Parameter  | Range            | Default | Original object |
|------------|------------------|---------|-----------------|
| Interval   | -36 … +36 st     | 0       | Pitch Hacker Dial (live.numbox "Interval") |
| Cents ("Fine") | -99 … +99 ct | 0       | live.numbox |
| Variation  | 0 … 24 st        | 0       | live.numbox |
| Rate       | 1/128 … 1/1D, 2/1, 3/1, 4/1 (24 steps) | 1/4 | live.dial |
| Reverse    | 0 … 100 %        | 0       | live.dial |
| Recycle    | 0 … 95 %         | 0       | live.dial |
| Level      | -96 … 0 dB       | 0       | live.dial |
| Dry / Wet  | 0 … 100 %        | 0       | live.dial |
| Restart    | button           | –       | hidden live.button (automatable) |

## Signal flow (as in the device)

Each Rate-length segment draws `Interval + Cents/100 + random(±Variation)`, rounds it (to whole
semitones if Cents = 0, else to 0.01 st), converts to a ratio, flips it negative with probability
*Reverse*, then ramps the read position of a 20 s delay line (`tapin~/tapout~`) so the segment
plays at that ratio, shaped by a 5 %/95 % trapezoid window. *Recycle* feeds the tap back into the
delay line (only when the ratio ≠ 1). Output → look-ahead limiter (2.5 ms / 500 ms / -0.1 dB) →
Level (5 ms ramp) → second limiter → linear Dry/Wet crossfade against the un-delayed input.
A segment restarts on plug-in load, on host transport start, and when *Restart* is triggered.

## Differences from the Max device (please read)

1. **Tempo** – the device converts the note value to ms (`translate notevalues ms`) only when the
   Rate knob is touched. Here the length is recomputed from the host tempo at every segment start,
   so tempo changes are followed.
2. **tapout~ minimum delay** – Max clamps `tapout~` to one signal vector; Live's vector size isn't
   knowable from a plug-in, so a fixed 64-sample minimum is used (`kMinTapDelaySamples`).
3. **Timing** – Max schedules segment changes on control-rate/vector boundaries; here they are sample accurate.
4. **Random numbers** – same distributions, different generator (fixed seed, so renders repeat).
5. **GUI** – same controls and layout, but redrawn in JUCE; the circular pitch-wheel artwork (SVG/jsui) is not reproduced.
6. `atodb~(0)` is assumed to floor at -999 dB and `rampsmooth~` to restart a linear ramp on every
   input change; these only affect limiter release shape on near-silent passages.

## Tests

    g++ -std=c++17 -O2 tests/dsp_test.cpp -o dsp_test && ./dsp_test

Checks +12/-12/+7 st land on the right frequencies, dry/wet 0 is bit-exact, the limiter holds its
ceiling, and every rate/tempo combination stays finite. The VST3 passes `pluginval` at strictness 5 and the CLAP passes `clap-validator`.

## Credits

Original device: *Pitch Hack* (Amazing Noises logo in the artwork). The embedded `vs.*` macros are
© Maurizio Giri / VirtualSound; this port re-implements the algorithms and contains none of their code.
Check the original's licence before redistributing.
