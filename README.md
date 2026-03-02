# Mulebox

A hardware guitar processing unit built with the Electrosmith Daisy Seed DSP module and Cleveland Audio Hothouse platform. It works as a reactive load box for silent playing, with cabinet simulation via Impulse Response files providing a line level, stereo output to e.g. speakers or a mixer. (It does not have a headphone amplifier built in).

This document provides guidance for how to build your own. Both software and hardware are open source. You can try to build it as described here, or use this as a starting point for your own build.

**WARNING #1:** The basic function of this box is to replace the speaker in an amplifier. A tube amplifier, in particular, will be damaged if used without a suitable load. Do not turn the amp on without a speaker or a load (like the one inside the Mulebox). If connections are made incorrectly, become loose, or are poorly soldered inside the Mulebox, the load could be disconnected. In this case, you could damage your amplifier.

**WARNING #2:** The components described here are for an 8 Ohm speaker output. Only plug into an 8 Ohm speaker output from your amplifier. The maximum rated power is 50W. It is safe to use a lower-powered amp, but do not use a higher-powered amplifier.

**WARNING #3:** This is a very tight build. You need to take care selecting components that can handle the required power, including wires. You need to be familar with soldering, guitar electronics (e.g. you have built guitar amps or pedals before), and be capable of building the code for this software project using basic development tools.

You will also need to supply your own IR files, e.g. ones you have bought yourself.

## Architecture

Think of the Mulebox as as two separate devices hardwired together:

1. An attenuator, using a variant of the ["JohnH" Attenuator](https://marshallforum.com/threads/simple-attenuators-design-and-testing.98285/) design. This provides a reactive load that is both safe and provides a an impedance curve that is similar to a speaker cabinet, which will make a tube amp "react" more appropriately.

The JohnH design can be used to build an atteunator box that quietens an amp whilst passing output to a real speaker in a guitar cabinet, with selectable levels of attenuation. The Mulebox does *not* support pass-through to a guitar cabinet. Instead it takes a "line out" signal and sends it to the DSP module.

The attenutator absorbs all the energy of the audio signal (up to 50W) and turns it into heat. Therefore, the attenuator gets hot. An aluminium chassis acts as a heat sink, and is cut with with plenty of ventilation. There is also a small fan to provide airflow. This is important not only to ensure the passive attenuator components don't fail – we are running a DSP module in the same box and this has a narrower operating temperature range.

2. A DSP processor that takes the raw amp sound and applies a selectable Impulse Response (IR) to it, emulating a guitar speaker cabinet, before passing the audio out as a stereo signal. This is based on the [Electrosmith Daisy Seed](https://electro-smith.com/products/daisy-seed) module, which can be programmed to perform a variety of audio-related functions (this is the source code repository for the Mulebox firmware, written in C++ for the Daisy platform).

The Daisy Seed needs to be connected to knobs and audio I/O. For that, we use the [Cleveland Music Co Hothouse](https://clevelandmusicco.com/hothouse/). This is a PCB that provides up six knobs, three switches, two footswitches, two LEDs, and stereo audio input and output, originally intended in a guitar pedal form factor. Mulebox uses the Hothouse PCB, but not the breakout boards for the jacks and footswitches and LEDs. This runs on 9V "pedal power".

Note that the attenuator is entirely passive. This means that if you turn your amp on but leave the Mulebox unpowered (there is no on/off switch), your amp is still safe. However, the cooling fan will not run, so it is advisable to leave it powered on whenever the amp is in use.

The Mulebox lives inside a "Hammond 1590DD" size enclosure – mainly because this is the largest enclosure one can order from Tayda Electronics and have them paint and drill. It is of course possible to use a different box, so long as it has the required cutouts and enough ventilation, but we provide Tayda drill and UV print templates below, which make it much easier to get a professional-looking enclosure.

## Hardware components

When selecting hardware components to use, it is important to consider not only their values (e.g. resistance and power rating/wattage for the resistors; inductance for the indctor coil), but also the physical dimensions of each. The Tayda drill template provided will need to be modified for any components with a different footprint from the ones suggested below. This is most important when it comes to the large, high-powered resistors. Since these are clamped to the bottom (technically the "lid") of the enclosure, it might be preferable to drill these manually.

### The Daisy Seed

First, buy the [Daisy Seed](electro-smith.com/products/daisy-seed). The Hothouse does not come with it. You can buy it from Elecrosmith directly, or a third party seller close to you. It can be reprogrammed as many times as you like and used for other projects (e.g. using the Hothouse platform in its default pedal-sized enclosure).

### The Hothouse PCB

Next, you need the [Hothouse](https://clevelandmusicco.com/hothouse-diy-digital-signal-processing-platform-kit/), which is what the Daisy Seed will mount to to gain access to audio I/O, knobs (pots), and LEDs.

Ideally, buy the Hothouse from Cleveland Music Co. They sell a version without an enclosure, which will save you a bit of money, whilst still supporting the developers of the Hothouse.

However, the fine folks at Cleveland Music Co have made the PCB available as [open hardware](https://github.com/clevelandmusicco/open-source-pedals/tree/main/hothouse) which means that you can also manufacture your own from their "Gerber" files – probably using [JLCPCB](https://jlcpcb.com). In this case, you want the main PCB only (not the footswitch or I/O breakout boards), and you probably want to get JLCPCB (or whoever you use) to provide and solder the SMD components for you.

There are several online tutorials that explain how to do this, but the basic idea is that you upload the Gerber (.zip) and Bill of Materials (.csv) file for the main PCB to the JLCPCB website, and choose their PCB assembly service. It's a very slick process. You will probably have to order at least five boards, but even then it's pretty affordable (plus you now have spares).

If you go this route, you will also need to buy two o single-row, 20-pin, 2.54mm pitch female square-pin headers and a 100uF low-ESR electrolytic capacitor, which are otherwise included in the Hothouse kit.

You will also need pots, jacks, a power socket, and LEDs, but we'll detail those below, as you'll need to get them regardless of whether you buy the Hothouse kit or manufacture your own PCBs. The components that come with the Hothouse are different from what we will mount in the Mulebox.

#### PCB mounts

The Hothouse PCB is meant to hang from its pots and switches, but for the Mulebox we'll need to mount these to the front and back of the chassis and wire them to the board. That means we need another way to mount the PCB, without having it touch the chassis itself. Unfortunately, there are no mounting holes on the Hothouse either.

Instead, we can use four Essentra TCEHCBS-4-01 PCB mounts and some #6 screws (or M3.5 size metric ones). These snap around the corners of the board. The drill template below is spaced for these. They are available from many places, including (with screws) [The Pi Hut](https://thepihut.com/products/14-corner-edge-standoffs-set-of-4).

### The inductor coil

[Sount imports](https://www.soundimports.eu/en/dayton-audio-lw18-90.html)

### The large resistors

TODO

### The cooling fan

TODO

### The enclosure

TODO

### Other components and shopping list

You need the components below. We suggest ordering everything that is available from Tayda Electronics if you are going to also have them drill and print the enclosure, because their components are cheap and their shipping is better done in bulk. Other components are listed from Farnell. You can of course choose whatever supplier you want, and use equivalent components, though you may then need to adjust some of the enclosure cutouts and other aspects of the layout.

#### Tayda Electonics

(SKU numbers in brackets)

* 1 x Red 5mm LED (A-1554)
* 1 x Blue 5mm LED (A-3005)
* 2 x 5mm LED bezels (A-660)
* 2 x B10K mini pots with solder lugs (A-1970)
* 1 x B5K mini pot with solder lugs (A-1970)
* 3 x dust seals for pots, unless supplied with pots (A-1310)
* 3 x small knobs for 6mm splined pots, ~16mm (e.g. A-6071)
* 1 x 100uF electrolytic capacitor (SKU A-959) – unless supplied with a full Hothouse Kit
* 2 x 20pin female headers (A-1310) – unless supplied with a full Hothouse kit
* 11 x 1K 1/4W metal film resistors (A-2200) – used for the rotary switch resistor ladder
* 1 x 1K 1W metal film resistor (A-2196) – higher power rating, used for the attenuator circuit 
* 3 x isolated jack sockets (A-6042)
* 1 x small isolated DC power socket (A-991)
* 4 x 10mm M3 bolts (A-1252) and nuts (A-1247) – for mounting 25W resistors
* 4 x 10mm M4 bolts (A-4351) and nuts (A-4357) – for mounting 100W resistor
* 4 x 25mm M3 bolts (A-1257) and nuts (A-1247) – for mounting cooling fan and finger grille

And for the enclosure:

(skip all these if using a different/custom-drilled enclosure)

* 1 x 1590DD enclosure in a suitable colour (e.g. A-5906)
* 1 x 1590DD custom drill service (A-5141-CST-DR1)
* 1 x 1590DD lid custom drill service (A-5141-CST-DRL) – you can skip this if you prefer to place and drill your own mounting holes for the large resistors, induction coil, and PCB
* 64 x additional hole drilling service (A-99999-CST-DR-H1) – adjust this so that the total number of cutouts in the enclosure is covered (40 holes are included in the default drilling service, so 40 + 64 = 104 total holes)
* 1 x 1590DD side C UV print service (A-5141-CST-UVC)
* 1 x 1590DD side E UV print service (A-5141-CST-UVE)
* 1 x gloss layer UV print service (A-99999-CST-UV-GL) – optional, but recommended to protect the printed labels
* 1 x "print twice" white layer UV print service (A-99999-CST-UV-WH) – optoinal, but recommended if printing white on black

#### Farnell

(Order codes in brackets)

* 4 x TCEHCBS-6-01 PCB mounts (4691573) – plus suitable screws (#6 or M3.5, self-tapping)
* 1 x 12-position make-before-break rotary switch – Lorlin CK1034 (1123690)
* 1 x 3K3 2W metal film resistor - used for the attenuator circuit (1738645)
* 1 x Fan finger guard (1781195)
* 1 x 10 Ohm, 100W resistor (1174288)
* 1 x 100 Ohm, 25W resistor (2009323)
* 1 x 39 Ohm, 25W resistor (2805189)

#### Other

* * 1 x medium knob for 6.35mm D-shaft rotary switch, ~19mm

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
- Some M3 and M4 size bolts, washers, and nuts for mounting the PCB and the large resistors.

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
   cd Mulebox
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
  - Further adapted for Mulebox
- libDaisy: MIT License
- DaisySP: MIT License
- Eigen: MPL2 License

## Resources

- [Daisy Documentation](https://electro-smith.github.io/libDaisy/)
- [DaisySP Documentation](https://electro-smith.github.io/DaisySP/)
- [Cleveland Music Co. Hothouse](https://clevelandmusicco.com/hothouse/)
- [Hothouse Examples](https://github.com/clevelandmusicco/HothouseExamples)
