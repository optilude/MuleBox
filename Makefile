# MuleBox - Guitar Processing Unit Makefile
# For Electrosmith Daisy Seed on Cleveland Audio Hothouse platform

# Project Name
TARGET = MuleBox

# Sources
CPP_SOURCES = src/main.cpp \
              src/hothouse.cpp \
              src/ImpulseResponse/dsp.cpp \
              src/ImpulseResponse/ImpulseResponse.cpp

# Include paths
C_INCLUDES = -Isrc -IEigen

# Library Locations
LIBDAISY_DIR = libDaisy
DAISYSP_DIR = DaisySP

# Core location, and generic Makefile
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile

# Override default .bin with .hex for QSPI flash support
# The .bin format fails with QSPI because it tries to fill the
# 2GB+ gap between internal flash (0x08000000) and QSPI (0x90000000)
TARGET_BIN = $(TARGET).hex

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
	@echo "Common targets:"
	@echo "  make          - Build the project"
	@echo "  make clean    - Clean build files"
	@echo "  make clean-all- Clean all build files including libraries"
	@echo "  make program-dfu - Flash to Daisy via USB DFU (uses .hex format)"
	@echo "  make flash    - Alias for program-dfu"
	@echo ""
	@echo "Before flashing:"
	@echo "  1. Connect Daisy Seed via USB"
	@echo "  2. Hold BOOT button and press RESET"
	@echo "  3. Release both buttons"
	@echo "  4. Run 'make flash'"
	@echo ""
	@echo "Note: This project uses .hex format for flashing due to QSPI flash usage."
