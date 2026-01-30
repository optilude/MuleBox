# MuleBox

A hardware guitar processing unit built with the Electrosmith Daisy Seed DSP module and Cleveland Audio Hothouse platform.

## Hardware

- **Reactive Load Box**: Analog circuit producing line-level output
- **DSP**: Electrosmith Daisy Seed (STM32-based)
- **Platform**: Cleveland Audio Hothouse
  - Stereo audio I/O
  - 6 potentiometers
  - 3 three-way toggle switches
  - 2 footswitches with LEDs
  - 9V power input

## Hardware Wiring

### IR Selection - 12-Position Rotary Switch

The cabinet impulse response is selected using a 12-position rotary switch connected via resistor ladder to **KNOB_2** (potentiometer 2).

**Resistor Ladder Wiring:**
```
                    +3.3V (from Hothouse)
                      |
    Position 0  ------+
                      |
                     1kΩ
                      |
    Position 1  ------+
                      |
                     1kΩ
                      |
    Position 2  ------+
                      |
                      ...
                      |
                     1kΩ
                      |
    Position 11 ------+
                      |
                     1kΩ
                      |
                    To KNOB_2 ADC input
                      |
                    GND
```

**Component Values:**
- **Resistors**: 12× 1kΩ ±1% (metal film recommended for accuracy)
- **Total Resistance**: 12kΩ
- **Current Draw**: ~0.275mA (negligible)

**Expected Voltages per Position:**
| Position | IR Name | Voltage | Normalized ADC |
|----------|---------|---------|----------------|
| 0        | Slot 1  | 0.00V   | 0.000          |
| 1        | Slot 2  | 0.275V  | 0.083          |
| 2        | Slot 3  | 0.55V   | 0.167          |
| 3        | Slot 4  | 0.825V  | 0.250          |
| 4        | Slot 5  | 1.10V   | 0.333          |
| 5        | Slot 6  | 1.375V  | 0.417          |
| 6        | Slot 7  | 1.65V   | 0.500          |
| 7        | Slot 8  | 1.925V  | 0.583          |
| 8        | Slot 9  | 2.20V   | 0.667          |
| 9        | Slot 10 | 2.475V  | 0.750          |
| 10       | Slot 11 | 2.75V   | 0.833          |
| 11       | Slot 12 | 3.025V  | 0.917          |

**Note**: The firmware applies hysteresis to prevent jitter between adjacent positions. IR selection is saved to flash memory and restored on power-up.

## Current Status

The project implements cabinet simulation using impulse response convolution:
- **Bass boost EQ** on mono input (adjustable via KNOB_1)
- **IR convolution** for cabinet simulation (12 selectable via rotary switch on KNOB_2)
- **Dual mono stereo output**

## Building

### Prerequisites

1. **ARM GCC Toolchain**
   ```bash
   # macOS
   brew install --cask gcc-arm-embedded

   # Linux (Debian/Ubuntu)
   sudo apt-get install gcc-arm-none-eabi

   # Or download from: https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm
   ```

2. **dfu-util** (for flashing via USB)
   ```bash
   # macOS
   brew install dfu-util

   # Linux
   sudo apt-get install dfu-util
   ```

3. **Clone with submodules**
   ```bash
   git clone --recurse-submodules <repository-url>
   cd MuleBox
   ```

   If already cloned without submodules:
   ```bash
   git submodule update --init --recursive
   ```

### Build Commands

```bash
make                # Build the project
make clean          # Clean build files
make clean-all      # Clean including library builds
make help           # Show all available targets
```

## Flashing to Daisy Seed

1. Connect the Daisy Seed to your computer via USB
2. Enter DFU (bootloader) mode:
   - Hold down the **BOOT** button
   - Press and release the **RESET** button
   - Release the **BOOT** button
3. Flash the firmware:
   ```bash
   make program-dfu
   # or simply:
   make flash
   ```

**Tip**: You can also enter DFU mode by holding Footswitch 1 for 2 seconds when the device is running.

## Development

See [CLAUDE.md](CLAUDE.md) for detailed development documentation, API reference, and examples for AI-assisted development.

## Project Structure

```
MuleBox/
├── src/
│   ├── main.cpp         - Main application
│   ├── hothouse.h       - Hardware abstraction
│   └── hothouse.cpp     - Hardware implementation
├── libDaisy/            - Daisy hardware library (submodule)
├── DaisySP/             - DSP algorithms library (submodule)
├── Makefile             - Build configuration
├── CLAUDE.md            - AI development context
└── README.md            - This file
```

## License

- Main application code: TBD
- Hothouse abstraction layer: GPL-3.0 (Copyright 2024 Cleveland Music Co.)
- libDaisy: MIT License
- DaisySP: MIT License

## Resources

- [Daisy Documentation](https://electro-smith.github.io/libDaisy/)
- [DaisySP Documentation](https://electro-smith.github.io/DaisySP/)
- [Cleveland Music Co. Hothouse](https://clevelandmusicco.com/hothouse/)
- [Hothouse Examples](https://github.com/clevelandmusicco/HothouseExamples)
