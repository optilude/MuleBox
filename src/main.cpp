// MuleBox - Guitar Cabinet IR Processing Unit
// Electrosmith Daisy Seed on Cleveland Audio Hothouse platform
//
// Mono input (left channel) -> IR convolution -> Stereo output
//
// Controls:
//   KNOB_4: Output level (noon = unity, CCW = cut, CW = boost)
//   KNOB_5: Reverb mix (0% to 100%)
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

#include "PlateauNEVersio/Dattorro.hpp"
#include "PlateauNEVersio/dsp/delays/InterpDelay.hpp"

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
Parameter reverbMixParam;     // KNOB_5: reverb mix
DebouncedAnalogSwitch irSwitch;

// Constants
static const float BASS_BOOST_AMOUNT = 0.2f; // Slight bump (~1 o'clock position on old pot mapping)
static const float BASS_FREQ = 110.0f;
static const float BASS_Q = 0.7f;
constexpr int MAX_IR_POSITIONS = 12;
constexpr size_t MAX_IR_LENGTH = 4096;
constexpr uint32_t SAMPLE_RATE = 48000;
constexpr size_t AUDIO_BLOCK_SIZE = 128;
constexpr float CLIPPING_THRESHOLD = 0.95f;
constexpr uint32_t CLIPPING_BLINK_DURATION_TICKS = 100 * (SAMPLE_RATE / AUDIO_BLOCK_SIZE) / 1000;
constexpr int DEBOUNCE_MS = 500;

// Reverb constants (Dattorro Plate defaults)
static const float REV_TIME_SCALE = 1.007500f;
static const float REV_MAX_LFO_DEPTH = 16.0f;
static const float REV_MAX_TIME_SCALE = 4.0f;
static const float REV_INPUT_LOW_CUT_PITCH = 2.87f;
static const float REV_TANK_LOW_CUT_FREQ = 2.87f;
static const float REV_INPUT_HIGH_CUT_PITCH = 7.25f;
static const float REV_MOD_SHAPE = 0.25f;
static const float REV_DECAY = 0.8f;
static const float REV_TANK_DIFFUSION = 0.85f;
static const float REV_PRE_DELAY = 0.0f;
static const float REV_TANK_HIGH_CUT_FREQ = 0.725f * 10.0f; // Tone parameter (0-1) * Plate damp scale (10.0f)
static const float REV_MOD_SPEED = 0.1f * 8.0f;             // Speed parameter (0.1-0.5) * Mod speed scale (8.0f)
static const float REV_MOD_DEPTH = 0.1f * 15.0f;            // Depth parameter (0.1-0.5) * Mod depth scale (15.0f)

// Convolution engine constants
constexpr size_t PARTITION_SIZE = 128;  // Match AUDIO_BLOCK_SIZE
constexpr size_t FFT_SIZE = 256;        // 2 * PARTITION_SIZE
constexpr size_t MAX_PARTITIONS = (MAX_IR_LENGTH + PARTITION_SIZE - 1) / PARTITION_SIZE;

// Large convolution buffers in SDRAM (64KB each)
float DSY_SDRAM_BSS convIrFreqBuf[MAX_PARTITIONS * FFT_SIZE];
float DSY_SDRAM_BSS convFdlBuf[MAX_PARTITIONS * FFT_SIZE];

// DSP
Svf bassFilter;
IrLoader irLoader;
Dattorro reverb(SAMPLE_RATE, REV_MAX_LFO_DEPTH, REV_MAX_TIME_SCALE);

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
    float reverbMix = reverbMixParam.Process();
    float outputLevel = outputLevelParam.Process();

    // Pre-process: read mono input, apply bass boost, check input clipping
    float firIn[AUDIO_BLOCK_SIZE];
    for (size_t i = 0; i < size; i++) {
        float mono = in[0][i];

        // Input clipping detection
        if (fabsf(mono) > CLIPPING_THRESHOLD) {
            inputClippingDetected = true;
        }

        // Hardcoded slight bass boost via SVF peak filter
        bassFilter.Process(mono);
        firIn[i] = mono + (bassFilter.Peak() * BASS_BOOST_AMOUNT);
    }

    // IR convolution (FFT-based partitioned overlap-save)
    float firOut[AUDIO_BLOCK_SIZE];
    irLoader.ProcessBlock(firIn, firOut, size);

    // Apply reverb processing, mix, and output scaling directly in one loop
    for (size_t i = 0; i < size; i++) {
        float rIn = firOut[i];
        
        // Process handles L and R simultaneously and saves state internally
        reverb.process(rIn, rIn);
        
        // Linear dry/wet mix from reverbMix knob (0.0 to 1.0), combined with output level scaling
        float dryMix = 1.0f - reverbMix;
        float wetMix = reverbMix;
        float sampleL = ((rIn * dryMix) + (reverb.getLeftOutput() * wetMix)) * outputLevel;
        float sampleR = ((rIn * dryMix) + (reverb.getRightOutput() * wetMix)) * outputLevel;

        // Output clipping detection
        if (fabsf(sampleL) > CLIPPING_THRESHOLD) {
            outputClippingDetected = true;
        }

        // Stereo output (dual mono if Reverb processing changes)
        out[0][i] = sampleL;
        out[1][i] = sampleR;
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

    // KNOB_5: Reverb mix (0.0 = 100% dry, 1.0 = 100% wet)
    // Using an EXPONENTIAL curve to emulate a traditional "logarithmic / audio taper" pot response
    reverbMixParam.Init(hw.knobs[Hothouse::KNOB_5],
                   0.0f,
                   1.0f,
                   Parameter::EXPONENTIAL);

    // Initialize bass EQ filter
    bassFilter.Init(hw.AudioSampleRate());
    bassFilter.SetFreq(BASS_FREQ);
    bassFilter.SetRes(BASS_Q);

    // Initialise the Dattorro Reverb with Flick default parameters
    // Zero out the InterpDelay buffers used by the plate reverb (Dattorro SDRAM)
    for(int i = 0; i < 50; i++) {
        for(int j = 0; j < 144000; j++) {
            sdramData[i][j] = 0.0f;
        }
    }
    // Set hold to 1.0 or plate reverb won't work (defined in InterpDelay.hpp)
    hold = 1.0f;

    reverb.setSampleRate(hw.AudioSampleRate());
    reverb.setTimeScale(REV_TIME_SCALE);
    reverb.enableInputDiffusion(true);

    // Set low-cut filters
    reverb.setInputFilterLowCutoffPitch(REV_INPUT_LOW_CUT_PITCH);
    reverb.setTankFilterLowCutFrequency(REV_TANK_LOW_CUT_FREQ);

    // Hardcoded parameters
    reverb.setInputFilterHighCutoffPitch(REV_INPUT_HIGH_CUT_PITCH);
    reverb.setTankModShape(REV_MOD_SHAPE);

    // Default "plate" preset parameters
    reverb.setDecay(REV_DECAY);
    reverb.setTankDiffusion(REV_TANK_DIFFUSION);
    reverb.setPreDelay(REV_PRE_DELAY);
    reverb.setTankFilterHighCutFrequency(REV_TANK_HIGH_CUT_FREQ);
    reverb.setTankModSpeed(REV_MOD_SPEED);
    reverb.setTankModDepth(REV_MOD_DEPTH);

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
