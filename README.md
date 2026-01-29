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

## Current Status

The project currently implements a **simple audio passthrough** - input is passed directly to output without processing. This serves as a foundation for implementing guitar effects.

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
