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
//   LED_1 (red):  Always on. Blinks off on input/output clipping.
//   LED_2 (blue): On when IR loaded. Off on empty slot. Blinks during IR switch.

#include "hothouse.h"
#include "daisysp.h"
#include "hid/parameter.h"
#include "Filters/fir.h"
#include "ir_data.h"

#include <cmath>

using clevelandmusicco::Hothouse;

using daisy::Parameter;
using daisy::Led;
using daisy::SaiHandle;
using daisy::AudioHandle;

using daisysp::Svf;

// Helper class to debounce discrete selections from analog controls
class DebouncedAnalogSwitch {
  public:
    void Init(uint32_t debounceMs) {
        debounceMs_ = debounceMs;
        lastChangeTime_ = 0;
        stableValue_ = -1; // -1 indicates not yet initialized
        pendingValue_ = -1;
    }

    int Process(int rawValue) {
        uint32_t now = daisy::System::GetNow();

        // First run initialization
        if (stableValue_ == -1) {
            stableValue_ = rawValue;
            pendingValue_ = -1;
            return stableValue_;
        }

        if (rawValue != stableValue_) {
            if (rawValue != pendingValue_) {
                // Value has changed to something new
                pendingValue_ = rawValue;
                lastChangeTime_ = now;
            } else {
                // Value is holding at pendingValue_
                if (now - lastChangeTime_ > debounceMs_) {
                    // It has been stable long enough. Commit it.
                    stableValue_ = rawValue;
                    pendingValue_ = -1;
                }
            }
        } else {
            // We are back at the stable position
            pendingValue_ = -1;
        }
        return stableValue_;
    }

    int Value() const { return stableValue_; }

  private:
    uint32_t debounceMs_;
    uint32_t lastChangeTime_;
    int stableValue_;
    int pendingValue_;
};

/**
 * Hardware interface
 */

Hothouse hw;
DebouncedAnalogSwitch irSwitch;
Led ledRed, ledBlue;
Parameter outputLevelParam;   // KNOB_4: output level
Parameter bassParam;          // KNOB_5: bass boost/cut
Parameter irSelectorParam;   // KNOB_6: IR selector (resistor ladder)


/**
 * Fixed constants
 */
constexpr float SAMPLE_RATE = 48000.0f;
static const float BASS_FREQ = 110.0f;        // Bass EQ center frequency in Hz
static const float BASS_Q = 0.7f;             // Q factor (bandwidth)
constexpr int MAX_IR_POSITIONS = 12;           // Rotary positions supported by hardware
constexpr size_t MAX_IR_LENGTH = 8192;         // Max IR samples (170ms @ 48kHz)
constexpr size_t AUDIO_BLOCK_SIZE = 8;         // Samples per audio callback
constexpr float CLIPPING_THRESHOLD = 0.95f;    // Clipping detection threshold
constexpr uint32_t CLIPPING_BLINK_MS = 100;    // LED blink duration on clipping

/**
 * DSP Globals
 */
Svf bassFilter;
daisysp::FIR<MAX_IR_LENGTH, AUDIO_BLOCK_SIZE> firFilter;
int currentIrIndex = 0;         // Currently loaded IR
volatile bool irBypass = false;  // When true, skip IR convolution

/**
 * LED / status state
 */
volatile bool clippingDetected = false;
uint32_t clippingBlinkStart = 0;

/**
 * Load IR from QSPI flash into FIR filter
 *
 * Sets irBypass=true during reconfiguration for thread safety
 * (audio callback checks irBypass before calling ProcessBlock).
 *
 * The FIR SetIR() reads coefficients directly from QSPI memory-mapped
 * addresses, reverses them internally, and stores in its own buffer.
 */
void loadIr(int irIndex) {
    using namespace ImpulseResponseData;

    if (IR_COUNT == 0) {
        irBypass = true;
        return;
    }

    // Validate index
    if (irIndex < 0 || irIndex >= (int)IR_COUNT) {
        irIndex = 0;
    }

    // Bypass during reconfiguration to avoid audio glitches
    irBypass = true;

    const IRInfo& irInfo = ir_collection[irIndex];

    // Load IR into FIR filter (reverse=true: IR is time-forward, FIR needs time-reversed)
    firFilter.SetIR(irInfo.data, irInfo.length, true);

    currentIrIndex = irIndex;
    irBypass = false;
}


/**
 * Blink blue LED N times, then leave on/off based on whether an IR exists
 * at the given position. N = position + 1 (position 0 = 1 blink, etc.)
 */
void blinkIrPosition(int position) {
    int blinkCount = position + 1;
    bool hasIr = (position < (int)ImpulseResponseData::IR_COUNT);

    ledBlue.Set(0.0f);
    ledBlue.Update();
    hw.DelayMs(200);

    for (int i = 0; i < blinkCount; i++) {
        ledBlue.Set(1.0f); ledBlue.Update(); hw.DelayMs(150);
        ledBlue.Set(0.0f); ledBlue.Update(); hw.DelayMs(150);
    }

    // Leave LED on if IR loaded, off if empty slot
    ledBlue.Set(hasIr ? 1.0f : 0.0f);
    ledBlue.Update();
}

// Audio callback - processes audio samples in blocks
// Called at audio rate: 48kHz / 8 samples = 6kHz
void AudioCallback(AudioHandle::InputBuffer in,
                   AudioHandle::OutputBuffer out,
                   size_t size) {

    // Read control parameters (smoothed)
    float bassAmount = bassParam.Process();
    float outputLevel = outputLevelParam.Process();

    // Pre-process: read mono input, apply bass boost/cut, check input clipping
    float firIn[AUDIO_BLOCK_SIZE];
    for (size_t i = 0; i < size; i++) {
        float mono = in[0][i];

        // Input clipping detection
        if (fabsf(mono) > CLIPPING_THRESHOLD) {
            clippingDetected = true;
        }

        // Bass boost/cut via SVF peak filter
        // bassAmount: -3.0 (max cut) to +3.0 (max boost), 0.0 = flat
        bassFilter.Process(mono);
        firIn[i] = mono + (bassFilter.Peak() * bassAmount);
    }

    // IR convolution (ARM CMSIS-DSP optimized block processing) or bypass
    float firOut[AUDIO_BLOCK_SIZE];
    if (!irBypass) {
        firFilter.ProcessBlock(firIn, firOut, size);
    } else {
        for (size_t i = 0; i < size; i++) {
            firOut[i] = firIn[i];
        }
    }

    // Apply output level, check output clipping, write stereo output
    for (size_t i = 0; i < size; i++) {
        float sample = firOut[i] * outputLevel;

        // Output clipping detection
        if (fabsf(sample) > CLIPPING_THRESHOLD) {
            clippingDetected = true;
        }

        // Stereo output (dual mono)
        out[0][i] = sample;
        out[1][i] = sample;
    }
}

int main(void) {
    // Initialize the Hothouse hardware
    hw.Init(true); // max CPU speed
    hw.SetAudioBlockSize(AUDIO_BLOCK_SIZE);
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);

    // Initialize LEDs
    ledRed.Init(hw.seed.GetPin(Hothouse::LED_1), false);
    ledBlue.Init(hw.seed.GetPin(Hothouse::LED_2), false);

    // Turn on LED1 (red) immediately to indicate power-on
    ledRed.Set(1.0f);
    ledRed.Update();

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

    // KNOB_6: IR selector (12-position resistor ladder, 0-11)
    // Resistor ladder: 12 positions with 1k resistors, 0 to 11k total.
    // Wired in place of B10K pot; ADC reads 0.0-1.0 across 12 voltage steps.
    irSelectorParam.Init(hw.knobs[Hothouse::KNOB_6],
                         0.0f,
                         (float)(MAX_IR_POSITIONS - 1),
                         Parameter::LINEAR);

    // Initialize debounce for IR switch (100ms)
    irSwitch.Init(100);

    // Initialize bass EQ filter
    bassFilter.Init(hw.AudioSampleRate());
    bassFilter.SetFreq(BASS_FREQ);
    bassFilter.SetRes(BASS_Q);

    // Start ADC so we can read knob 6 before audio starts
    hw.StartAdc();

    // Read initial IR selection from knob 6 (resistor ladder)
    // Process controls and pump the parameter smoother to let it converge
    float rawValue = 0.0f;
    for (int i = 0; i < 50; i++) {
        hw.ProcessAllControls();
        rawValue = irSelectorParam.Process();
        hw.DelayMs(2);
    }
    {
        int rawPosition = (int)(rawValue + 0.5f);
        if (rawPosition < 0) rawPosition = 0;
        if (rawPosition >= MAX_IR_POSITIONS) rawPosition = MAX_IR_POSITIONS - 1;

        // Initialize debouncer with this position
        irSwitch.Process(rawPosition);

        // Load IR if valid, otherwise bypass
        if (rawPosition < (int)ImpulseResponseData::IR_COUNT) {
            loadIr(rawPosition);
        } else {
            irBypass = true;
        }

        // Blink to indicate position
        blinkIrPosition(rawPosition);
    }

    // Start audio processing
    hw.StartAudio(AudioCallback);

    // Main loop
    while(1) {

        // Process all hardware controls (knobs, switches)
        hw.ProcessAllControls();

        // IR selection from resistor ladder (KNOB_6)
        float rawValue = irSelectorParam.Process();
        int rawPosition = (int)(rawValue + 0.5f);  // Round to nearest integer

        // Clamp to selector's physical range (0..11)
        if (rawPosition < 0) rawPosition = 0;
        if (rawPosition >= MAX_IR_POSITIONS) rawPosition = MAX_IR_POSITIONS - 1;

        // Process through debouncer to get stable position
        int prevPosition = irSwitch.Value();
        int selectedPosition = irSwitch.Process(rawPosition);

        // Detect switch change (debounced)
        if (selectedPosition != prevPosition) {
            bool hasIr = (selectedPosition < (int)ImpulseResponseData::IR_COUNT);

            if (hasIr) {
                loadIr(selectedPosition);
            } else {
                irBypass = true;
            }

            // Blink N times then set steady state
            blinkIrPosition(selectedPosition);
        }

        // LED state machine
        uint32_t now = daisy::System::GetNow();

        // LED1 (red): always on, blinks off briefly on clipping
        float led1 = 1.0f;
        if (clippingDetected) {
            clippingBlinkStart = now;
            clippingDetected = false;
        }
        if (clippingBlinkStart > 0 && (now - clippingBlinkStart) < CLIPPING_BLINK_MS) {
            led1 = 0.0f;
        } else {
            clippingBlinkStart = 0;
        }
        ledRed.Set(led1);

        // Update LED hardware
        ledRed.Update();
        ledBlue.Update();

        // Small delay to prevent busy-waiting
        hw.DelayMs(10);
    }
}
