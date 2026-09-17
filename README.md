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

Everything is signed with a Developer ID and notarized, so nothing is blocked by Gatekeeper.

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

### Signing and notarizing the release

Releases are signed with a Developer ID and notarized, so macOS opens them without a warning.
Without these variables `make-dmg.sh` still produces a working but ad-hoc signed build that
users must right-click > Open. Store the notary credentials once (an App Store Connect API
key avoids app-specific passwords):

```bash
xcrun notarytool store-credentials arrowech-notary \
  --key ~/Downloads/AuthKey_KEYID.p8 --key-id KEYID --issuer ISSUER-UUID
```

Then build:

```bash
APP_SIGN_ID="Developer ID Application: Your Name (TEAMID)" \
INSTALLER_SIGN_ID="Developer ID Installer: Your Name (TEAMID)" \
NOTARY_PROFILE=arrowech-notary \
./scripts/make-dmg.sh
```

The plug-ins, the standalone app, the .pkg and the DMG are all signed with the hardened
runtime and a secure timestamp; Apple's notary service takes a few minutes per file, and the
script waits and staples the tickets so everything validates offline. Check a build with
`spctl -a -vvv -t install dist/ArrowEch-<version>.dmg` (expect `source=Notarized Developer ID`).

The standalone app carries `packaging/standalone.entitlements` (microphone access) — the
plug-ins don't need it, because the host owns the input.

Tests:

```bash
cmake --build build --target DSPSmokeTest && ./build/DSPSmokeTest_artefacts/Release/DSPSmokeTest
cmake --build build --target UISnapshot && ./build/UISnapshot_artefacts/Release/UISnapshot build
```
