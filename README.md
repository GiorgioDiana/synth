# Sixth

A six-voice, Juno-6-inspired synthesizer instrument built with C++17 and JUCE 8.0.12. Targets macOS Audio Unit and VST3, with a standalone app. Linux VST3 and standalone targets support development and testing.

Sixth is an independent analog-inspired design, not a measured circuit emulation or an official Roland product. The oscillator, resonant filter and chorus approximate the instrument's architecture; sound matching, authentic voice calibration and BBD chorus modelling remain future work.

## Play

Load Sixth on an instrument track, enable MIDI input, and play notes. The standalone app also has an on-screen keyboard. Six voices are available; additional notes steal older voices. Pitch bend spans two semitones.

Version 0.2 adds a resizable modern panel, grouped knobs, filter/envelope illustrations, an output meter and nine factory patches. Double-click a knob to reset it; Shift-drag for fine adjustments. Values can be typed with Hz/kHz, ms/s, cents or percent units. The filter display illustrates the base filter shape; it is not an audio spectrum analyzer.

The sound controls include:

- **Oscillator:** saw/pulse/sub/noise mixer, pulse width, 16'/8'/4' range, fine tune and per-voice analog drift.
- **Modulation:** sine, triangle, square or sample-and-hold LFO; independent pitch, PWM and filter depths; delayed fade-in and key retrigger. MIDI mod wheel (CC1) adds adjustable vibrato; pitch bend remains ±2 semitones.
- **Filter:** resonant 24 dB/octave low-pass, high-pass, pre-filter drive, bipolar envelope depth, keyboard tracking and velocity sensitivity.
- **Envelopes:** independent amplifier and filter ADSRs, so a long pad can have a fast filter pluck or a short note can open slowly.
- **Chorus/output:** modes Off/I/II, continuous mix/rate/depth, true stereo chorus width, amplitude velocity response and output level.

Factory patches cover strings, bass, brass, plucks, pads, leads, resonant steps and organ. The voice oscillators run at twice the output rate and use PolyBLEP saw/pulse waveforms. DAW sessions save every control and the selected factory patch. Existing v0.1 parameter IDs and order are retained; old shared-envelope sessions are migrated into matching amp/filter envelopes. Expanded chorus/drift processing means this is not a bit-for-bit reconstruction of the old render. Hardware SysEx, arpeggiator, mono/glide/unison modes and user-preset file import/export are not implemented yet. Chord performance is described below.

## Chord performance (v0.3)

Open **CHORDS**, choose a style and harmony bank, and enable **CHORD MODE**. Each chromatic input key selects a different chord in that bank. MIDI 48–59 (C3–B3 here) matches the twelve displayed pads; the next keyboard octave plays the same slots an octave higher. Hosts use different octave labels, so the MIDI numbers are authoritative. You can also hold a pad with the mouse to play it. The regular keyboard mode remains available by switching chord mode off.

There are **48 banks with 576 mapped chord slots**, all written for four to six voices. See the [complete bank catalogue](docs/ChordBanks.md). Common harmonies intentionally recur across related palettes. The library includes 10 funk/neo-soul banks, 6 Bill Evans-inspired rootless/modal banks, 8 functional jazz banks, 8 Bach-inspired chorale banks, 8 Chopin-inspired Romantic banks, 4 gospel/soul banks and 4 modal/cinematic banks.

These are original style-inspired harmonic palettes, not transcriptions of named works. Chorale banks provide four-part harmonic choices; arbitrary key order is not an automatic counterpoint or voice-leading composition system. Rootless jazz banks intentionally omit roots, leaving room for a separate bass part.

**Key** transposes the whole palette. **Voicing** offers Written, Close, Open, Drop 2, Rotate 1 and Rotate 2. Written keeps each bank's curated spacing; the other modes rearrange the same chord tones and preserve doubled voices. Rotate options cycle existing voices rather than claiming formal root-based inversions. **Register** shifts all chords by octaves, in addition to the register selected by your input key. At MIDI limits, complete voicings fold by octaves to retain their notes.

For an immediate example, choose **Bach / Chorale → Chorale I - Major Cadences**, Key C and Written voicing. Play C3 (48), D#3 (51), E3 (52), C3 (48) for C–F–G7–C. In **Evans / Rootless Major A**, D3 (50), D#3 (51), C3 (48), B3 (59) gives Dm9–G13–Cmaj9–C6/9. The pad labels show the actual chord and its notes, so each bank's different mapping is visible.

Harmony banks are independent from the **Sound Patch** browser: changing timbre preserves the chosen bank, key, voicing and chord-mode setting. DAW sessions save both. Held keys retain the notes captured when they were pressed, even if you change the bank before releasing them. Shared common tones remain held until their last input key is released. Sustain, pitch bend, mod wheel and MIDI channels are preserved; **PANIC** stops held/sustained notes. Switching chord mode releases existing voices. Chord mode uses up to 24 voices for overlapping voicings; normal mode retains the six-voice behavior. Extreme overlap can still steal older voices.

The expansion happens inside Sixth: the DAW records incoming trigger keys, not an exported stream of generated chord MIDI. Chord mode is disabled by default when loading older sessions, and its AU parameters follow the published v0.1/v0.2 controls to preserve their automation order.


## Build on your Mac

Install Xcode (including its command-line tools) and CMake 3.22 or newer. Clone/download this repository and run:

```sh
bash scripts/build-macos.sh
```

CMake downloads the pinned JUCE source unless `JUCE_SOURCE_DIR` already points to a local copy. The script builds universal Apple Silicon/Intel binaries, signs them ad-hoc for local testing, runs tests, and creates `build-macos/Sixth-macOS.pkg`.

The outputs are under `build-macos/Sixth_artefacts/Release/`:

- `AU/Sixth.component`
- `VST3/Sixth.vst3`
- `Standalone/Sixth.app`

For local use, copy the component to `~/Library/Audio/Plug-Ins/Components/` and the VST3 bundle to `~/Library/Audio/Plug-Ins/VST3/` (create the directories if needed). Restart your DAW or rescan its plugins. Logic uses the Audio Unit. Ableton, Cubase and other VST3 hosts can use VST3. The standalone app is useful for trying the sound without a DAW; choose an audio output and MIDI input in its audio settings.

The GitHub `macOS plugins` workflow builds these outputs on pushes and manual runs, verifies both architectures, signs them ad-hoc, runs the tests and Apple's `auval` validator, and packages an installer. Download the `Sixth-macOS` artifact from a successful workflow. Open the enclosed `Sixth-macOS.pkg` on your Mac to install the AU and VST3 in `/Library/Audio/Plug-Ins/` and the standalone app in `/Applications/`. The artifact also contains a tar archive for manual installation. The installer requires administrator approval. These builds use ad-hoc signatures, not Developer ID signing or notarization; macOS may require you to explicitly allow the installer in Privacy & Security. Native build and DAW results must be checked from an actual workflow run; Linux tests do not establish them. Developer ID signing, notarization and testing in your target DAW remain required before distribution.

## Cloud / Linux development

In the prepared cloud environment:

```sh
source /workspace/.synth-env/activate.sh
export CMAKE_BUILD_PARALLEL_LEVEL=4
cmake -S /workspace/synth -B /workspace/.synth-env/synth-build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build /workspace/.synth-env/synth-build --target SynthTests Sixth_Standalone -j4
ctest --test-dir /workspace/.synth-env/synth-build --output-on-failure
```

On another Linux machine, install GCC/Clang, CMake, Ninja, pkg-config and JUCE's ALSA, freetype/fontconfig and X11 development dependencies, then configure with `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release`. Browser, curl and OpenGL support are unused.

The tests validate all chord banks/voicings/transpositions, mapped MIDI lifetimes and channel/timing behavior, actual chord audio, UI pad lifecycle and state recall, as well as octave frequency ratios, independent filter envelopes, filter LFO, velocity, chorus mix/width, level-matched drive, audible factory patches, legacy session migration, polyphonic rendering/release, editor painting, state restoration and scanning/loading the actual VST3 bundle. They do not establish acoustic fidelity to a hardware Juno-6 or Mac DAW compatibility.

## Licensing

JUCE has its own licensing terms (AGPLv3 or a commercial JUCE licence); review those before distributing a product. Third-party notices are included in JUCE's source. No application-source licence has been chosen for this repository yet.
