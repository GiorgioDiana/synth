# Sixth

A six-voice, Juno-6-inspired synthesizer instrument built with C++17 and JUCE 8.0.12. Targets macOS Audio Unit and VST3, with a standalone app. Linux VST3 and standalone targets support development and testing.

Sixth is an independent analog-inspired design, not a measured circuit emulation or an official Roland product. The oscillator, resonant filter and chorus approximate the instrument's architecture; sound matching, authentic voice calibration and BBD chorus modelling remain future work.

## Play

Load Sixth on an instrument track, enable MIDI input, and play notes. The standalone app also has an on-screen keyboard. Normal mode has six voices; chord mode has up to 24 for overlapping voicings. Additional notes can steal older voices. Pitch bend spans two semitones.

The synth page provides a resizable modern panel, grouped knobs, filter/envelope illustrations, an output meter and nine factory sound patches. Double-click a knob to reset it; Shift-drag for fine adjustments. Values can be typed with Hz/kHz, ms/s, cents or percent units. The filter display illustrates the base filter shape; it is not an audio spectrum analyzer.

The sound controls include:

- **Oscillator:** saw/pulse/sub/noise mixer, pulse width, 16'/8'/4' range, fine tune and per-voice analog drift.
- **Modulation:** sine, triangle, square or sample-and-hold LFO; independent pitch, PWM and filter depths; delayed fade-in and key retrigger. MIDI mod wheel (CC1) adds adjustable vibrato; pitch bend remains ±2 semitones.
- **Filter:** resonant 24 dB/octave low-pass, high-pass, pre-filter drive, bipolar envelope depth, keyboard tracking and velocity sensitivity.
- **Envelopes:** independent amplifier and filter ADSRs, so a long pad can have a fast filter pluck or a short note can open slowly.
- **Chorus/output:** modes Off/I/II, continuous mix/rate/depth, true stereo chorus width, amplitude velocity response and output level.

Factory patches cover strings, bass, brass, plucks, pads, leads, resonant steps and organ. The voice oscillators run at twice the output rate and use PolyBLEP saw/pulse waveforms. DAW sessions save every control and the selected factory patch. Existing v0.1 parameter IDs and order are retained; old shared-envelope sessions are migrated into matching amp/filter envelopes. Expanded chorus/drift processing means this is not a bit-for-bit reconstruction of the old render. Hardware SysEx, arpeggiator, mono/glide/unison modes and user-preset file import/export are not implemented yet. Chord performance is described below.

## Chord performance (v0.4.1)

Open **CHORDS**, select a style/bank and enable **CHORD MODE**. All **49 chord pads are visible together**, covering **C1 through C5, inclusive (MIDI 24–72)**. Each key has its own written voicing across the bank's broader harmonic palette. Hold a pad with the mouse or play its key on a MIDI controller. Selecting a different bank enables its full 49-key map. Notes outside that range still play as individual melody notes through MIDI.

There are **48 banks × 49 keys = 2,352 mapped chord slots**, with six voicing modes and transposition to all twelve keys. Shared chords and different voicings intentionally recur; this is a slot count, not a claim that every harmony is unique. Styles include 10 funk/neo-soul banks, 6 Bill Evans-inspired jazz banks, 8 functional jazz banks, 8 Bach-inspired chorale banks, 8 Chopin-inspired Romantic banks, 4 gospel/soul banks and 4 modal/cinematic banks. See the [full mapping catalogue](docs/ChordBanks.md).

Each palette now includes clear major/minor tonic landings, predominant/turnaround routes, the original bank’s characteristic chords, and borrowed/chromatic bridges. Style is the centre of a bank, not a restriction to one chord type. The central C3–B3 range (MIDI 48–59) preserves its original twelve choices; the other ranges have their own harmonic options. **C1 is a dependable home and B1 is a dominant leading back to it. C5 gives another tonic voicing.** Try those exits when exploring a more colourful part of the map.

For an immediate example, choose **Bach / Chorale → Chorale I - Major Cadences**, Key C and Written voicing. C3 (48) → D#3 (51) → E3 (52) → C3 (48) plays C–F–G7–C; C1 (24) or C5 (72) is also available as a tonic landing. In **Evans / Rootless Major A**, D3 → D#3 → C3 → B3 plays Dm9–G13–Cmaj9–C6/9, while its new surrounding ranges provide rooted homes and more routes onward.

**Key** transposes the whole bank. **Voicing** offers Written, Close, Open, Drop 2, Rotate 1 and Rotate 2; these rearrange the same chord tones and retain doubled voices. Rotate options cycle existing voices rather than promising a formal root-based inversion. **Register** moves the actual sounding voicing by octaves. In Full mode, moving to another input octave selects different harmony rather than automatically raising the same twelve chords. At extreme MIDI limits, complete voicings fold by octaves to retain all notes.

### See what you are playing

- **Amber** marks the input/trigger key.
- **Teal** marks the notes sounding in the chord, including notes generated by the pads.
- A **split colour** marks a key that is both input and output.
- The chord/input readout identifies the actual played chord and its trigger note/MIDI number, whether you use pads or a MIDI controller, on either the Synth or Chords page.

The keyboard always displays the complete C1–C5 chord range. Its scale stays fixed while you play, release notes, use the sustain pedal or change registers. Generated notes outside that displayed range are listed in the chord readout. Resize the plugin window to enlarge the whole panel.

Harmony banks remain independent from the **Sound Patch** browser: changing timbre preserves the bank, key, voicing and mode. DAW sessions save both. Held keys release the chord captured when they were pressed, even across a bank change; shared common tones remain held until their last input owner releases. Sustain, pitch bend, mod wheel and MIDI channels are preserved. **PANIC** stops held/sustained notes; switching chord mode releases existing voices. Chord mode provides up to 24 voices; normal mode retains six. Extreme overlap can still steal older voices.

### Existing sessions

Projects saved before v0.4.1 now open with **Full C1–C5**, including projects that previously selected Legacy12 automatically. The original central C3–B3 octave is preserved; other trigger octaves now select the expanded harmonies. **Legacy12** remains an explicit compatibility choice for replaying the old twelve-slot octave pattern. A Legacy12 choice saved in v0.4.1 is recalled, while selecting a different bank switches to its full 49-key map. Chord mode remains off when restoring older non-chord sessions, and existing AU parameter order is retained.

These are original style-inspired harmonic palettes, not transcriptions of named works or performances. Chorale banks retain a four-part core but can include colours from other styles; arbitrary key order is not an automatic counterpoint composition system. Rootless jazz choices intentionally leave room for a bass part. Chord expansion happens inside Sixth: the DAW normally records incoming trigger keys, not an exported generated-MIDI stream.

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

The tests validate expanded and legacy chord maps, voicings/transpositions, mapped MIDI lifetimes and channel/timing behavior, actual chord audio, UI pad lifecycle, input/output note feedback and state recall, as well as octave frequency ratios, independent filter envelopes, filter LFO, velocity, chorus mix/width, level-matched drive, audible factory patches, legacy session migration, polyphonic rendering/release, editor painting, state restoration and scanning/loading the actual VST3 bundle. They do not establish acoustic fidelity to a hardware Juno-6 or Mac DAW compatibility.

## Licensing

JUCE has its own licensing terms (AGPLv3 or a commercial JUCE licence); review those before distributing a product. Third-party notices are included in JUCE's source. No application-source licence has been chosen for this repository yet.
