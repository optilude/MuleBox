# MuleBox - Guitar Processing Unit Makefile
# For Electrosmith Daisy Seed on Cleveland Audio Hothouse platform

# Project Name
TARGET = MuleBox

# Sources
CPP_SOURCES = src/main.cpp \
              src/hothouse.cpp

# CMSIS-DSP FIR filtering (ARM-optimized convolution for IR processing)
C_SOURCES = $(LIBDAISY_DIR)/Drivers/CMSIS-DSP/Source/FilteringFunctions/arm_fir_f32.c \
            $(LIBDAISY_DIR)/Drivers/CMSIS-DSP/Source/FilteringFunctions/arm_fir_init_f32.c

# Include paths
C_INCLUDES = -Isrc

# Enable ARM DSP optimizations for DaisySP FIR filter
C_DEFS = -DUSE_ARM_DSP

# Library Locations
LIBDAISY_DIR = libDaisy
DAISYSP_DIR = DaisySP

# Use Daisy bootloader: application is written to QSPI flash via DFU,
# then copied to SRAM at boot. This enables programming both firmware
# and IR data in a single DFU flash operation.
APP_TYPE = BOOT_SRAM

# Use the 2000ms grace period bootloader so that firmware can be
# reflashed by pressing RESET and running make program-dfu within 2.5s
BOOT_BIN = $(SYSTEM_FILES_DIR)/dsy_bootloader_v6_3-intdfu-2000ms.bin

# Core location, and generic Makefile
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile

# Override libDaisy's `program` target (which errors out for BOOT_SRAM)
# to flash firmware directly to QSPI via the STLINK debug probe.
# Only the debug probe is needed — no USB, no buttons.
#
# Uses openocd_daisy_qspi.cfg to initialize the QUADSPI peripheral
# and IS25LP064A flash chip, then writes the binary to 0x90040000.
.PHONY: program program-boot-probe

program:
	@echo "Flashing firmware to QSPI via debug probe..."
	$(OCD) -s $(OCD_DIR) \
		-f $(PGM_DEVICE) \
		-c "set QUADSPI 1" \
		-f target/$(CHIPSET).cfg \
		-f openocd_daisy_qspi.cfg \
		-c "init" \
		-c "reset init" \
		-c "program $(BUILD_DIR)/$(TARGET_BIN) verify reset exit $(QSPI_ADDRESS)"

# Flash the Daisy bootloader to internal flash via debug probe.
# No buttons required — just connect STLINK and run this target.
program-boot-probe:
	@echo "Flashing Daisy bootloader via debug probe..."
	$(OCD) -s $(OCD_DIR) $(OCDFLAGS) \
		-c "program $(BOOT_BIN) verify reset exit $(INTERNAL_ADDRESS)"

# Additional targets for convenience
.PHONY: clean-all flash help

# Clean everything including libraries
clean-all: clean
	$(MAKE) -C $(LIBDAISY_DIR) clean
	$(MAKE) -C $(DAISYSP_DIR) clean

# Alias for program-dfu
flash: program-dfu

# Help target
help:
	@echo "MuleBox Build System"
	@echo "===================="
	@echo ""
	@echo "First-time setup (flash Daisy bootloader):"
	@echo "  Option A - USB only:"
	@echo "    1. Connect Daisy Seed via USB"
	@echo "    2. Hold BOOT button and press RESET, release both"
	@echo "    3. Run 'make program-boot'"
	@echo "  Option B - Debug probe (no buttons needed):"
	@echo "    1. Connect STLINK debug probe"
	@echo "    2. Run 'make program-boot-probe'"
	@echo ""
	@echo "Flashing firmware:"
	@echo "  Option A - USB only:"
	@echo "    1. Enter Daisy bootloader (press RESET, or hold left footswitch 2s)"
	@echo "    2. Run 'make program-dfu' (or 'make flash') within 2.5 seconds"
	@echo "  Option B - Debug probe (no USB or buttons needed):"
	@echo "    1. Connect STLINK debug probe (power via 9V or USB)"
	@echo "    2. Run 'make program'"
	@echo ""
	@echo "Common targets:"
	@echo "  make                  - Build the project"
	@echo "  make clean            - Clean build files"
	@echo "  make clean-all        - Clean including library builds"
	@echo "  make program-boot     - Flash bootloader via USB DFU (first-time)"
	@echo "  make program-dfu      - Flash firmware via USB DFU"
	@echo "  make flash            - Alias for program-dfu"
	@echo "  make program          - Flash firmware via debug probe (QSPI direct)"
	@echo "  make program-boot-probe - Flash bootloader via debug probe"
