// MuleBox - Guitar Cabinet IR Processing Unit
// Electrosmith Daisy Seed on Cleveland Audio Hothouse platform
//
// Mono input (left channel) -> IR convolution -> Stereo output
//
// Controls:
//   KNOB_4: Output level (noon = unity, CCW = cut, CW = boost)
//   KNOB_5: Bass boost/cut (noon = flat, CCW = cut, CW = boost)
//   KNOB_6: IR selector (12-position rotary switch via resistor ladder)
//
// LEDs:
//   LED_1 (red):  Always on. Blinks on startup for IR count. Blinks off on clipping.
//   LED_2 (blue): On when IR loaded. Off on empty slot. Blinks position on IR switch.

#include "hothouse.h"
#include "daisysp.h"
#include "hid/parameter.h"
#include "debounced_analog_switch.h"
#include "ir_loader.h"
#include "ir_data.h"
#include "led_controller.h"
#include "dev/sdram.h"

using clevelandmusicco::Hothouse;

using daisy::Parameter;
using daisy::Led;
using daisy::SaiHandle;
using daisy::AudioHandle;

using daisysp::Svf;

// Hardware
Hothouse hw;
Led ledRed, ledBlue;
Parameter outputLevelParam;   // KNOB_4: output level
Parameter bassParam;          // KNOB_5: bass boost/cut
DebouncedAnalogSwitch irSwitch;

// Constants
static const float BASS_FREQ = 110.0f;
static const float BASS_Q = 0.7f;
constexpr int MAX_IR_POSITIONS = 12;
constexpr size_t MAX_IR_LENGTH = 8192;
constexpr uint32_t SAMPLE_RATE = 48000;
constexpr size_t AUDIO_BLOCK_SIZE = 128;
constexpr float CLIPPING_THRESHOLD = 0.95f;
constexpr uint32_t CLIPPING_BLINK_DURATION_TICKS = 100 * (SAMPLE_RATE / AUDIO_BLOCK_SIZE) / 1000;
constexpr int DEBOUNCE_MS = 500;

// Convolution engine constants
constexpr size_t PARTITION_SIZE = ConvolutionEngine::L;  // 128
constexpr size_t FFT_SIZE = ConvolutionEngine::N;        // 256
constexpr size_t MAX_PARTITIONS = (MAX_IR_LENGTH + PARTITION_SIZE - 1) / PARTITION_SIZE;

// Large convolution buffers in SDRAM (64KB each)
float DSY_SDRAM_BSS convIrFreqBuf[MAX_PARTITIONS * FFT_SIZE];
float DSY_SDRAM_BSS convFdlBuf[MAX_PARTITIONS * FFT_SIZE];

// DSP
Svf bassFilter;
IrLoader irLoader;

// State
volatile bool isLoadingIr = false;
volatile bool inputClippingDetected = false;
volatile bool outputClippingDetected = false;

LedController redLedController(ledRed, SAMPLE_RATE, AUDIO_BLOCK_SIZE);
LedController blueLedController(ledBlue, SAMPLE_RATE, AUDIO_BLOCK_SIZE);

void blinkLedBlocking(Led& led, int times, bool keep_on = false, float brightness = 1.0f, int delay_ms = 250) {
    led.Set(0.0f); led.Update(); hw.DelayMs(200);
    for (int i = 0; i < times; i++) {
        led.Set(brightness); led.Update(); hw.DelayMs(delay_ms);
        if (i < times - 1 || !keep_on) {
            led.Set(0.0f); led.Update(); hw.DelayMs(delay_ms);
        }
    }
}

// Audio callback - processes audio samples in blocks
// Called at audio rate: 48kHz / 128 samples = 375Hz
void AudioCallback(AudioHandle::InputBuffer in,
                   AudioHandle::OutputBuffer out,
                   size_t size) {

    // Process rotary switch at audio rate for reliable debouncing
    // Skip during IR loading to avoid thread-safety issues
    if (!isLoadingIr) {
        irSwitch.Process();
        blueLedController.SetBaseBrightness(irLoader.irBypass ? 0.0f : 1.0f);
    }

    // Process LEDs at audio rate
    redLedController.ProcessAudioRate();
    blueLedController.ProcessAudioRate();

    // Read control parameters (smoothed)
    float bassAmount = bassParam.Process();
    float outputLevel = outputLevelParam.Process();

    // Pre-process: read mono input, apply bass boost/cut, check input clipping
    float firIn[AUDIO_BLOCK_SIZE];
    for (size_t i = 0; i < size; i++) {
        float mono = in[0][i];

        // Input clipping detection
        if (fabsf(mono) > CLIPPING_THRESHOLD) {
            inputClippingDetected = true;
        }

        // Bass boost/cut via SVF peak filter
        // bassAmount: -3.0 (max cut) to +3.0 (max boost), 0.0 = flat
        bassFilter.Process(mono);
        firIn[i] = mono + (bassFilter.Peak() * bassAmount);
    }

    // IR convolution (FFT-based partitioned overlap-save)
    float firOut[AUDIO_BLOCK_SIZE];
    irLoader.ProcessBlock(firIn, firOut, size);

    // Apply output level, check output clipping, write stereo output
    for (size_t i = 0; i < size; i++) {
        float sample = firOut[i] * outputLevel;

        // Output clipping detection
        if (fabsf(sample) > CLIPPING_THRESHOLD) {
            outputClippingDetected = true;
        }

        // Stereo output (dual mono)
        out[0][i] = sample;
        out[1][i] = sample;
    }
}

int main(void) {
    hw.Init(true);
    hw.SetAudioBlockSize(AUDIO_BLOCK_SIZE);
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);

    // Initialize LEDs
    ledRed.Init(hw.seed.GetPin(Hothouse::LED_1), false);
    ledBlue.Init(hw.seed.GetPin(Hothouse::LED_2), false);

    // Initialize rotary switch (KNOB_6: 12-position resistor ladder)
    irSwitch.Init(hw.knobs[Hothouse::KNOB_6], MAX_IR_POSITIONS, DEBOUNCE_MS);

    // KNOB_4: Output level (0.0 = silence, noon = unity, full CW = +6dB boost)
    outputLevelParam.Init(hw.knobs[Hothouse::KNOB_4],
                          0.0f,
                          2.0f,
                          Parameter::LINEAR);

    // KNOB_5: Bass boost/cut (-3.0 to +3.0, noon = 0.0 = flat)
    bassParam.Init(hw.knobs[Hothouse::KNOB_5],
                   -3.0f,
                   3.0f,
                   Parameter::LINEAR);

    // Initialize bass EQ filter
    bassFilter.Init(hw.AudioSampleRate());
    bassFilter.SetFreq(BASS_FREQ);
    bassFilter.SetRes(BASS_Q);

    // Start ADC so we can read knobs before audio starts
    hw.StartAdc();

    // Blink red LED for each loaded IR on startup, then keep on
    blinkLedBlocking(ledRed, ImpulseResponseData::IR_COUNT, true);
    redLedController.SetBaseBrightness(1.0f);
    
    // Stabilize rotary switch reading (let ADC + debounce converge)
    for (int i = 0; i < 60; i++) {
        irSwitch.Process();
        hw.DelayMs(10);
    }
    irSwitch.ResetChangedFlag();

    // Initialize convolution engine (FFT setup, SDRAM buffers)
    irLoader.Init(MAX_PARTITIONS, convIrFreqBuf, convFdlBuf);

    // Load initial IR so we have audio immediately
    int initialPosition = irSwitch.Value();
    if (initialPosition >= 0 && initialPosition < (int)ImpulseResponseData::IR_COUNT) {
        irLoader.loadIr(initialPosition);
    } else {
        irLoader.irBypass = true;
    }

    // Blink blue LED to indicate the initial IR position as audio starts
    blueLedController.Blink(initialPosition + 1, false, 1.0f, 200);

    // Start audio processing
    hw.StartAudio(AudioCallback);

    // Main loop
    while (1) {

        // Check for IR switch changes (irSwitch.Process() runs in audio callback)
        if (irSwitch.HasChanged()) {
            isLoadingIr = true;
            irSwitch.ResetChangedFlag();

            int position = irSwitch.Value();

            // Blink blue LED to indicate IR position (dimmer blink)
            blueLedController.Blink(position + 1, false, 1.0f, 200);

            // Load IR or set bypass for empty slot
            if (position >= 0 && position < (int)ImpulseResponseData::IR_COUNT) {
                irLoader.loadIr(position);
            } else {
                irLoader.irBypass = true;
            }

            isLoadingIr = false;
        }

        // LED1 (red): always on, blinks off briefly on input clipping
        if (inputClippingDetected) {
            inputClippingDetected = false;
            redLedController.InterruptBlink(CLIPPING_BLINK_DURATION_TICKS);
        } else {
            redLedController.SetBaseBrightness(1.0f);
        }

        // LED2 (blue): on when loaded / off when empty, blinks opposite on output clipping
        if (outputClippingDetected) {
            outputClippingDetected = false;
            float blinkTarget = irLoader.irBypass ? 1.0f : 0.0f;
            blueLedController.InterruptBlink(CLIPPING_BLINK_DURATION_TICKS, blinkTarget);
        }

        hw.DelayMs(50);
    }
}
