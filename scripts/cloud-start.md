# Sixth cloud startup

Use the existing /workspace/synth checkout. Every task is already isolated; do not create a Git worktree unless explicitly requested.

The project is a six-voice Juno-6-inspired JUCE instrument. The Linux workflow builds and tests VST3 plus standalone; macOS AU/VST3 builds require Xcode on a Mac. Read README.md for supported features, limitations and Mac installation.

Initialize each build/test shell:

```bash
source /workspace/.synth-env/activate.sh
export CMAKE_BUILD_PARALLEL_LEVEL=4
cd /workspace/synth
```

If initialization is missing, run `bash /workspace/synth/scripts/cloud-install.sh` first. This retains the compiler from the host and installs CMake/Ninja, pinned JUCE 8.0.12 and a signed Debian dependency overlay in /workspace/.synth-env, outside the checkout. Do not set CMAKE_SYSROOT; use the activation script's include/library/pkg-config paths.

Build and validate:

```bash
cmake -S /workspace/synth -B /workspace/.synth-env/synth-build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build /workspace/.synth-env/synth-build --target SynthTests Sixth_Standalone -j4
ctest --test-dir /workspace/.synth-env/synth-build --output-on-failure
```

Require `synth_audio_midi_state` to execute and pass. It renders MIDI chords, checks release, finite audio at three sample rates, state restoration, filter/chorus stability, then scans and loads the built VST3. This does not verify hardware sound matching or Mac DAW compatibility.

No service, credential or audio device is required for headless tests. For interactive use, the standalone binary is `/workspace/.synth-env/synth-build/Sixth_artefacts/Release/Standalone/Sixth`. It needs a running desktop and an audio output for live playback. Do not assume a desktop or audio device is available. Processes must be restarted in a new task.

Mac users can run `bash scripts/build-macos.sh` or use the provided GitHub macOS workflow after committing/pushing the code. Native Mac binaries, AU validation and DAW behavior have not been verified in this Linux environment.
