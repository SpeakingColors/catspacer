# Cat Spacer 🐈

<img src="assets/icon.svg" width="96" align="right" alt="icon">

Free, open source spectral sidechain ducker for Linux (VST3 and standalone). The sidechain's spectrum carves matching gaps in the main signal so a lead and a vocal or kick can share the mix.

## Features
- 64 bands (30 Hz to 20 kHz), STFT, no threshold needed: it follows the loudest sidechain band and ducks the bands that are prominent
- Amount, Low cut, High cut, Attack, Release, Pan, L/R or M/S, Freeze (holds the sidechain curve), Max cut, Floor, Mix
- Live display: sidechain spectrum, main spectrum and the cut applied (reacts to every control)
- Presets and Randomize
- Reported latency: 2048 samples (host compensates)

## Status
Early scaffold. It builds in CI (Linux VST3 and standalone) but has not been tuned by ear yet. Free and open source. Donations welcome (link coming).

## Docs
- [Roadmap](ROADMAP.md)

## Build
CMake 3.22+, a C++17 compiler and the usual JUCE Linux libraries. `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build`. JUCE and the shared cat-ui look are fetched automatically.

## License
AGPL-3.0-or-later, see [LICENSE](LICENSE).

Original STFT design with a sidechain input. Not affiliated with any other ducking plugin; no code or artwork from them.
