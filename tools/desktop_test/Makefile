# Desktop test harness for MuleBox IR convolution engine.
# Builds on macOS using Apple Accelerate framework (vDSP) as FFT backend.
#
# Usage:
#   make            Build the test tool
#   make test       Build and run self-test
#   make clean      Remove build artifacts

CXX = clang++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter
FRAMEWORKS = -framework Accelerate

# Include our arm_math.h shim FIRST (so it shadows the real <arm_math.h>),
# then the production source headers.
INCLUDES = -I. -I../../src

BUILD_DIR = ../../build
TARGET = $(BUILD_DIR)/mulebox_ir_test

DEPS = mulebox_ir_test.cpp \
       arm_math.h \
       ../../src/convolution_engine.h \
       ../../src/ir_loader.h \
       ../../src/ir_data.h

.PHONY: all test clean

all: $(TARGET)

$(TARGET): $(DEPS)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(FRAMEWORKS) mulebox_ir_test.cpp -o $@

test: $(TARGET)
	$(TARGET) --selftest

clean:
	rm -f $(TARGET)
