# MuleBox - Guitar Processing Unit Makefile
# For Electrosmith Daisy Seed on Cleveland Audio Hothouse platform

# Project Name and Sources
ifdef TEST
  ifeq ($(TEST),knobs)
    TARGET = test_knobs
    APP_SRC = src/tests/test_knobs.cpp
  else ifeq ($(TEST),rotary)
    TARGET = test_rotary
    APP_SRC = src/tests/test_rotary.cpp
  else ifeq ($(TEST),ir_loading)
    TARGET = test_ir_loading
    APP_SRC = src/tests/test_ir_loading.cpp
  else
    $(error Unknown test: $(TEST). Use TEST=knobs, TEST=rotary, or TEST=ir_loading)
  endif
else
  TARGET = MuleBox
  APP_SRC = src/main.cpp
endif

# Sources
CPP_SOURCES = $(APP_SRC) \
              src/hothouse.cpp

# CMSIS-DSP FFT-based convolution (partitioned overlap-save for IR processing)
C_SOURCES = $(LIBDAISY_DIR)/Drivers/CMSIS-DSP/Source/TransformFunctions/arm_rfft_fast_f32.c \
            $(LIBDAISY_DIR)/Drivers/CMSIS-DSP/Source/TransformFunctions/arm_rfft_fast_init_f32.c \
            $(LIBDAISY_DIR)/Drivers/CMSIS-DSP/Source/TransformFunctions/arm_cfft_f32.c \
            $(LIBDAISY_DIR)/Drivers/CMSIS-DSP/Source/TransformFunctions/arm_cfft_init_f32.c \
            $(LIBDAISY_DIR)/Drivers/CMSIS-DSP/Source/TransformFunctions/arm_cfft_radix8_f32.c \
            $(LIBDAISY_DIR)/Drivers/CMSIS-DSP/Source/TransformFunctions/arm_bitreversal2.c \
            $(LIBDAISY_DIR)/Drivers/CMSIS-DSP/Source/CommonTables/arm_common_tables.c \
            $(LIBDAISY_DIR)/Drivers/CMSIS-DSP/Source/CommonTables/arm_const_structs.c \
            $(LIBDAISY_DIR)/Drivers/CMSIS-DSP/Source/ComplexMathFunctions/arm_cmplx_mult_cmplx_f32.c \
            $(LIBDAISY_DIR)/Drivers/CMSIS-DSP/Source/BasicMathFunctions/arm_add_f32.c

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

# Ensure the IR header is generated before compiling any sources
$(OBJECTS): src/ir_data.h

src/ir_data.h: tools/wav_to_ir_header.py $(wildcard irs/*.wav)
	@echo "Rebuilding IR header and binary from irs/*.wav..."
	@mkdir -p build
	$(PYTHON) tools/wav_to_ir_header.py irs/*.wav -o src/ir_data.h --bin build/ir_data.bin

# Override libDaisy's `program` target (which errors out for BOOT_SRAM)
# to flash firmware directly to QSPI via the STLINK debug probe.
# Only the debug probe is needed — no USB, no buttons.
#
# Uses openocd_daisy_qspi.cfg to initialize the QUADSPI peripheral
# and IS25LP064A flash chip, then writes the binary to 0x90040000.
#
# Also flashes the IR data binary to 0x90080000.
.PHONY: program program-boot-probe

program: src/ir_data.h
	@echo "Creating combined binary..."
	cp $(BUILD_DIR)/$(TARGET_BIN) build/combined.bin
	@# Pad to 512KB (524288 bytes) to match IR_DATA_START_OFFSET in python script
	$(PYTHON) -c "import os; f=open('build/combined.bin','ab'); size=os.path.getsize('build/combined.bin'); pad=524288-size; f.write(b'\0'*pad) if pad>0 else None"
	cat build/ir_data.bin >> build/combined.bin
	@echo "Flashing combined firmware (APP + IRs) to QSPI..."
	$(OCD) -s $(OCD_DIR) \
		-f $(PGM_DEVICE) \
		-c "set QUADSPI 1" \
		-f target/$(CHIPSET).cfg \
		-f openocd_daisy_qspi.cfg \
		-c "init" \
		-c "reset init" \
		-c "program build/combined.bin verify reset exit 0x90040000"

# Flash the Daisy bootloader to internal flash via debug probe.
# No buttons required — just connect STLINK and run this target.
program-boot-probe:
	@echo "Flashing Daisy bootloader via debug probe..."
	$(OCD) -s $(OCD_DIR) $(OCDFLAGS) \
		-c "program $(BOOT_BIN) verify reset exit $(INTERNAL_ADDRESS)"

# Desktop test harness (builds and runs on macOS, not Daisy hardware)
.PHONY: desktop-test
desktop-test: src/ir_data.h
	$(MAKE) -C tools/desktop_test

# Additional targets for convenience
.PHONY: clean-all flash help update-irs

PYTHON ?= python3

# Rebuild IR header from WAV files manually if needed
update-irs: src/ir_data.h

src/ir_data.h: tools/wav_to_ir_header.py $(wildcard irs/*.wav)
	@echo "Rebuilding IR header from irs/*.wav..."
	@mkdir -p build
	$(PYTHON) tools/wav_to_ir_header.py irs/*.wav -o src/ir_data.h --bin build/ir_data.bin

# Clean everything including libraries
clean-all: clean
	$(MAKE) -C $(LIBDAISY_DIR) clean
	$(MAKE) -C $(DAISYSP_DIR) clean

# Additional clean step
clean:
	rm -f src/ir_data.h

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
	@echo "  make TEST=knobs       - Build the test_knobs program"
	@echo "  make TEST=rotary      - Build the test_rotary program"
	@echo "  make update-irs       - Rebuild IR header from WAV files"
	@echo "  make clean            - Clean build files"
	@echo "  make clean-all        - Clean including library builds"
	@echo "  make program-boot     - Flash bootloader via USB DFU (first-time)"
	@echo "  make program-dfu      - Flash firmware via USB DFU"
	@echo "  make flash            - Alias for program-dfu"
	@echo "  make program          - Flash firmware via debug probe (QSPI direct)"
	@echo "  make program-boot-probe - Flash bootloader via debug probe"
