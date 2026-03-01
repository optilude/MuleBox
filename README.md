# MuleBox

A hardware guitar processing unit built with the Electrosmith Daisy Seed DSP module and Cleveland Audio Hothouse platform. It works as a reactive load box for silent playing, with cabinet simulation via Impulse Response files providing a line level, stereo output to e.g. speakers or a mixer. (It does not have a headphone amplifier built in).

This document provides guidance for how to build your own. Both software and hardware are open source. You can try to build it as described here, or use this as a starting point for your own build.

**WARNING #1:** The basic function of this box is to replace the speaker in an amplifier. A tube amplifier, in particular, will be damaged if used without a suitable load. Do not turn the amp on without a speaker or a load (like the one inside the MuleBox). If connections are made incorrectly, become loose, or are poorly soldered inside the MuleBox, the load could be disconnected. In this case, you could damage your amplifier.

**WARNING #2:** The components described here are for an 8 Ohm speaker output. Only plug into an 8 Ohm speaker output from your amplifier. The maximum rated power is 50W. It is safe to use a lower-powered amp, but do not use a higher-powered amplifier.

**WARNING #3:** This is a very tight build. You need to take care selecting components that can handle the required power, including wires. You need to be familar with soldering, guitar electronics (e.g. you have built guitar amps or pedals before), and be capable of building the code for this software project using basic development tools.

You will also need to supply your own IR files, e.g. ones you have bought yourself.

## Architecture

Think of the MuleBox as as two separate devices hardwired together:

1. An attenuator, using a variant of the ["JohnH" Attenuator](https://marshallforum.com/threads/simple-attenuators-design-and-testing.98285/) design. This provides a reactive load that is both safe and provides a an impedance curve that is similar to a speaker cabinet, which will make a tube amp "react" more appropriately.

The JohnH design can be used to build an atteunator box that quietens an amp whilst passing output to a real speaker in a guitar cabinet, with selectable levels of attenuation. The MuleBox does *not* support pass-through to a guitar cabinet. Instead it takes a "line out" signal and sends it to the DSP module.

The attenutator absorbs all the energy of the audio signal (up to 50W) and turns it into heat. Therefore, the attenuator gets hot. An aluminium chassis acts as a heat sink, and is cut with with plenty of ventilation. There is also a small fan to provide airflow. This is important not only to ensure the passive attenuator components don't fail – we are running a DSP module in the same box and this has a narrower operating temperature range.

2. A DSP processor that takes the raw amp sound and applies a selectable Impulse Response (IR) to it, emulating a guitar speaker cabinet, before passing the audio out as a stereo signal. This is based on the [Electrosmith Daisy Seed](https://electro-smith.com/products/daisy-seed) module, which can be programmed to perform a variety of audio-related functions (this is the source code repository for the MuleBox firmware, written in C++ for the Daisy platform).

The Daisy Seed needs to be connected to knobs and audio I/O. For that, we use the [Cleveland Music Co Hothouse](https://clevelandmusicco.com/hothouse/). This is a PCB that provides up six knobs, three switches, two footswitches, two LEDs, and stereo audio input and output, originally intended in a guitar pedal form factor. MuleBox uses the Hothouse PCB, but not the breakout boards for the jacks and footswitches and LEDs. This runs on 9V "pedal power".

Note that the attenuator is entirely passive. This means that if you turn your amp on but leave the MuleBox unpowered (there is no on/off switch), your amp is still safe. However, the cooling fan will not run, so it is advisable to leave it powered on whenever the amp is in use.

The MuleBox lives inside a "Hammond 1590DD" size enclosure – mainly because this is the largest enclosure one can order from Tayda Electronics and have them paint and drill. It is of course possible to use a different box, so long as it has the required cutouts and enough ventilation, but we provide Tayda drill and UV print templates below, which make it much easier to get a professional-looking enclosure.

## Hardware components

When selecting hardware components to use, it is important to consider not only their values (e.g. resistance and power rating/wattage for the resistors; inductance for the indctor coil), but also the physical dimensions of each. The Tayda drill template provided will need to be modified for any components with a different footprint from the ones suggested below. This is most important when it comes to the large, high-powered resistors. Since these are clamped to the bottom (technically the "lid") of the enclosure, it might be preferable to drill these manually.

### The Daisy Seed

### The Hothouse PCB and PCB mounts

### The indictor coil

### The large resistors

### The cooling fan

### Other components

### The enclosure

### Tools and consumables

You will need:

- A good quality soldering iron with both a pencil tip for PCB soldering and a lager tip for the large resistors, jacks, and so on.
- Other soldering tools – high quality solder, an extractor fan and/or mask to avoid breathing in fumes, a solder sucker for rework, helping hands or clamps, etc.
- Screwdrivers, pliers, wirecutters, wire strippers, spanners in various sizes to affix pots to the enclosure, etc.
- A multimeter that can measure resistance and continuity (beep mode)
- 22 AWG (i.e. thick) wires, preferably in a few different colours, for wiring the attenuator.
- Thinner wire in muiltiple colours for wiring up the Hothouse to the output jacks, LEDs, and potentiometers.
- Plastic zipties for mounting the inductor coil
- Thermal paste (only a tiny amount) for moutning the large resistors

## Assembly

## Preparing your IRs

## Building the software

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


## License

- Project license: GPL-3.0 (see `LICENSE`)
- Main application code: GPL-3.0, unless otherwise noted
- Hothouse abstraction layer: GPL-3.0 (Copyright 2024 Cleveland Music Co.)
- Impulse Response loader (`src/ImpulseResponse/`): MIT License
  - Original code from [NeuralAmpModeler Plugin](https://github.com/sdatkinson/NeuralAmpModelerPlugin) by Steven Atkinson
  - Modified by Keith Bloemer for the [Mars effect in FunBox](https://github.com/GuitarML/FunBox/tree/main/software/Mars)
  - Further adapted for MuleBox
- libDaisy: MIT License
- DaisySP: MIT License
- Eigen: MPL2 License

## Resources

- [Daisy Documentation](https://electro-smith.github.io/libDaisy/)
- [DaisySP Documentation](https://electro-smith.github.io/DaisySP/)
- [Cleveland Music Co. Hothouse](https://clevelandmusicco.com/hothouse/)
- [Hothouse Examples](https://github.com/clevelandmusicco/HothouseExamples)
