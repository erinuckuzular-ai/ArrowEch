# ArrowEch

A goofy, very functional multi-effect rack for macOS: VST3, AU and Standalone, Apple Silicon and Intel, macOS 11+.
A cartoon blob explains every control (badly) and judges your settings.

## Download

Get **ArrowEch.dmg** from the [latest release](../../releases/latest). Every push to `main` builds a fresh DMG automatically.

Open the DMG and double-click **Install ArrowEch.pkg**. The installer puts:

- VST3 in `/Library/Audio/Plug-Ins/VST3`
- AU in `/Library/Audio/Plug-Ins/Components`
- the standalone app in `/Applications`

Click **Customize** to choose which ones. Then rescan plug-ins in your DAW.

If macOS says it can't check the installer for malicious software, right-click it and choose **Open**, or go to System Settings > Privacy & Security > **Open Anyway**. This happens because the installer isn't signed with an Apple Developer ID.

## What's in the rack

The signal flows through 10 modules. Drag the chips at the top to reorder them.

| Module | What it does |
|---|---|
| **The Echo-er** | Tempo-synced or free delay. Styles: Digital, Tape, Analog, Lo-Fi, Diffuse, Dub Spring. Modes: Single, Dual, Ping-Pong. Filters, saturation, wow/flutter, smear, ducking, width, reverb on the repeats, **FREEZE**, and a **THROW** send for dub throws. |
| **Wobble Twins** | Stereo detune thickener (Snug / Wide / Seasick) |
| **S-S-Stutter** | Bar-synced slice repeater |
| **Grain Salad** | Granular delay with scatter, pitch and **reverse** |
| **Crunch-o-matic** | Bitcrusher with rhythmic gating |
| **Fuzz Bucket** | Saturation: Tube, Transistor, Tape, Fuzz, Broken, plus a **PUNISH** button |
| **Wah Goblin** | Resonant filter swept by a synced LFO and an envelope follower |
| **Seasick** | Tempo-synced tremolo and auto-pan with chop, spread and randomness |
| **Swoosh-a-tron** | Tempo-synced phaser |
| **Squish** | Compressor with parallel mix and sidechain-style pump |

There are also 31 factory presets (including a dub section), **ROLL DICE** to randomize everything, **OH NO** to clear all tails, and input, output and rack-mix controls.

## Build locally

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release        # builds and installs to ~/Library/Audio/Plug-Ins
./scripts/make-dmg.sh                        # universal DMG in dist/ (needs ~2 GB free)
```

CMake fetches JUCE 8 automatically.

Tests:

```bash
cmake --build build --target DSPSmokeTest && ./build/DSPSmokeTest_artefacts/Release/DSPSmokeTest
cmake --build build --target UISnapshot && ./build/UISnapshot_artefacts/Release/UISnapshot build
```
