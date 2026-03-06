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
	@echo "  1. Connect Daisy Seed via USB"
	@echo "  2. Hold BOOT button and press RESET, release both"
	@echo "  3. Run 'make program-boot'"
	@echo ""
	@echo "Flashing firmware:"
	@echo "  1. Enter Daisy bootloader (hold left footswitch 2s, or press RESET)"
	@echo "  2. Run 'make program-dfu' (or 'make flash')"
	@echo ""
	@echo "Common targets:"
	@echo "  make              - Build the project"
	@echo "  make clean        - Clean build files"
	@echo "  make clean-all    - Clean including library builds"
	@echo "  make program-boot - Flash Daisy bootloader (first-time only)"
	@echo "  make program-dfu  - Flash firmware via USB DFU"
	@echo "  make flash        - Alias for program-dfu"
