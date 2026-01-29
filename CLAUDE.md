# MuleBox - AI Development Context

This document provides context for AI assistants (like Claude) to help evolve this codebase effectively.

## Project Overview

**MuleBox** is a hardware guitar processing unit combining:
- Analog reactive load box (line-level output)
- Electrosmith Daisy Seed DSP module (STM32 microcontroller)
- Cleveland Audio Hothouse platform (pedal interface)

The Hothouse provides:
- Stereo audio in/out
- 6 potentiometers (knobs)
- 3 three-way toggle switches (on-off-on)
- 2 footswitches with LED indicators
- 9V power input

## Project Structure

```
MuleBox/
├── src/
│   ├── main.cpp         - Main application and audio callback
│   ├── hothouse.h       - Hothouse hardware abstraction (from Cleveland Music Co.)
│   └── hothouse.cpp     - Hothouse implementation
├── libDaisy/            - Git submodule: Daisy hardware abstraction layer
├── DaisySP/             - Git submodule: DSP library with effects/filters
├── Makefile             - Build configuration
├── CLAUDE.md            - This file
└── README.md            - User-facing documentation (create as needed)
```

## Current Implementation

The current code implements a **simple passthrough** - audio input is copied directly to output without processing. This serves as a foundation for adding effects.

### Audio Flow
1. Audio callback runs at sample rate / block size (default: 48kHz / 48 samples = 1kHz)
2. Input buffer contains stereo samples: `in[0]` (left), `in[1]` (right)
3. Output buffer: `out[0]` (left), `out[1]` (right)
4. Currently: `out[i][j] = in[i][j]` (passthrough)

### Main Loop
- Processes hardware controls (knobs, switches)
- Checks for bootloader reset (hold footswitch 1 for 2 seconds)
- Runs continuously at ~100Hz (10ms delay)

## Hardware API Reference

### Hothouse Class

**Initialization:**
```cpp
hw.Init();                          // Initialize hardware
hw.SetAudioBlockSize(48);           // Samples per callback
hw.StartAudio(AudioCallback);       // Start processing
```

**Audio Control:**
```cpp
hw.AudioSampleRate();               // Get sample rate (Hz)
hw.AudioBlockSize();                // Get block size
hw.AudioCallbackRate();             // Callback frequency (Hz)
```

**Input Controls:**
```cpp
// Knobs (0.0 to 1.0)
float value = hw.GetKnobValue(Hothouse::KNOB_1);  // KNOB_1 through KNOB_6

// Toggle Switches
auto pos = hw.GetToggleswitchPosition(Hothouse::TOGGLESWITCH_1);
// Returns: TOGGLESWITCH_UP, TOGGLESWITCH_MIDDLE, TOGGLESWITCH_DOWN

// Must call in main loop:
hw.ProcessAllControls();  // Updates knob/switch states
```

**Footswitches:**
```cpp
// Simple polling:
if (hw.switches[Hothouse::FOOTSWITCH_1].RisingEdge()) { /* ... */ }

// Advanced (double/long press):
Hothouse::FootswitchCallbacks callbacks = {
    .HandleNormalPress = OnNormalPress,
    .HandleDoublePress = OnDoublePress,
    .HandleLongPress = OnLongPress
};
hw.RegisterFootswitchCallbacks(&callbacks);
```

**LEDs:**
```cpp
hw.seed.SetLed(Hothouse::LED_1, brightness);  // 0.0 to 1.0
hw.seed.SetLed(Hothouse::LED_2, brightness);
```

## DaisySP Library

The DaisySP library provides pre-built DSP algorithms. Common modules:

**Filters:**
- `Svf` - State variable filter (LP, HP, BP, Notch)
- `Tone` - Simple tone control
- `OnePole` - Single pole LP/HP filter

**Effects:**
- `Chorus` - Chorus effect
- `Overdrive` - Distortion/overdrive
- `Tremolo` - Amplitude modulation
- `Decimator` - Bit crushing
- `Compressor` - Dynamic range compression

**Reverb/Delay:**
- `ReverbSc` - Stereo reverb
- `DelayLine` - Delay buffer

**Oscillators:**
- `Oscillator` - Various waveforms
- `AdEnv` - Attack/decay envelope

Usage pattern:
```cpp
#include "daisysp.h"
using namespace daisysp;

Overdrive overdrive;
overdrive.Init();
overdrive.SetDrive(0.5f);  // 0.0 to 1.0
float out = overdrive.Process(in);
```

## Building and Flashing

**First-time setup:**
```bash
# Ensure arm-none-eabi-gcc toolchain is installed
# On macOS: brew install --cask gcc-arm-embedded

# Initialize submodules (already done)
git submodule update --init --recursive
```

**Build:**
```bash
make                # Build project
make clean          # Clean build files
make clean-all      # Clean including libraries
```

**Flash to Daisy:**
```bash
# 1. Connect Daisy Seed via USB
# 2. Enter DFU mode: Hold BOOT, press RESET, release both
# 3. Flash:
make program-dfu    # or: make flash
```

## Development Workflow

When adding new features:

1. **Define the feature** - What should it do? Which controls?
2. **Add state variables** - Before `AudioCallback` function
3. **Initialize** - In `main()` before `StartAudio()`
4. **Process controls** - Map knobs/switches to parameters
5. **Process audio** - Implement DSP in `AudioCallback()`
6. **Test incrementally** - Build and test each step

### Example: Adding an Overdrive Effect

```cpp
#include "daisysp.h"
using namespace daisysp;

Hothouse hw;
Overdrive overdrive;  // Add effect module

void AudioCallback(AudioHandle::InputBuffer in,
                   AudioHandle::OutputBuffer out,
                   size_t size) {
    // Get drive amount from knob 1
    float drive = hw.GetKnobValue(Hothouse::KNOB_1);
    overdrive.SetDrive(drive);

    for (size_t i = 0; i < size; i++) {
        // Process left channel
        out[0][i] = overdrive.Process(in[0][i]);
        // Process right channel (or copy left for mono)
        out[1][i] = overdrive.Process(in[1][i]);
    }
}

int main(void) {
    hw.Init();

    // Initialize overdrive
    overdrive.Init();

    hw.SetAudioBlockSize(48);
    hw.StartAudio(AudioCallback);

    while(1) {
        hw.ProcessAllControls();
        hw.CheckResetToBootloader();
        hw.DelayMs(10);
    }
}
```

## Common Patterns

### Bypass/Wet-Dry Mix

```cpp
bool bypass = false;

void AudioCallback(...) {
    for (size_t i = 0; i < size; i++) {
        float wet = ProcessEffect(in[0][i]);
        out[0][i] = bypass ? in[0][i] : wet;
    }
}
```

### True Bypass with Footswitch

```cpp
// In main loop:
if (hw.switches[Hothouse::FOOTSWITCH_2].RisingEdge()) {
    bypass = !bypass;
    hw.seed.SetLed(Hothouse::LED_2, bypass ? 0.0f : 1.0f);
}
```

### Parameter Smoothing

```cpp
OnePole smoother;
smoother.Init();
smoother.SetFrequency(10.0f);  // 10 Hz smoothing

// In audio callback or main loop:
float raw = hw.GetKnobValue(Hothouse::KNOB_1);
float smooth = smoother.Process(raw);
```

## Next Steps / Ideas

Potential features to implement:
- [ ] Overdrive/distortion with tone control
- [ ] Compressor with threshold and ratio controls
- [ ] Reverb with room size and decay
- [ ] Delay with time and feedback
- [ ] Multi-effect chain with routing
- [ ] Preset system (save/load settings)
- [ ] MIDI control
- [ ] Expression pedal input
- [ ] True analog bypass switching

## Resources

- [Daisy Documentation](https://electro-smith.github.io/libDaisy/)
- [DaisySP Documentation](https://electro-smith.github.io/DaisySP/)
- [Hothouse Examples](https://github.com/clevelandmusicco/HothouseExamples)
- [Daisy Forum](https://forum.electro-smith.com/)

## Notes for AI Assistants

- Always read [main.cpp](src/main.cpp) before suggesting modifications
- Hothouse hardware abstraction is GPL-3.0 licensed
- Audio callback runs in real-time - avoid heavy processing
- Sample rate is typically 48kHz, block size 48 samples
- Use DaisySP modules when available (well-tested, optimized)
- Test on hardware for timing/performance issues
- Control processing happens in main loop, not audio callback
- Preserve the simple structure unless complexity is needed
