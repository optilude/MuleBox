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

using clevelandmusicco::Hothouse;

using daisy::Parameter;
using daisy::Led;
using daisy::SaiHandle;
using daisy::AudioHandle;

using daisysp::Svf;

// Hardware
Hothouse hw;
DebouncedAnalogSwitch irSwitch;
Led ledRed, ledBlue;
Parameter outputLevelParam;   // KNOB_4: output level
Parameter bassParam;          // KNOB_5: bass boost/cut

// Constants
static const float BASS_FREQ = 110.0f;
static const float BASS_Q = 0.7f;
constexpr int MAX_IR_POSITIONS = 12;
constexpr size_t MAX_IR_LENGTH = 8192;
constexpr size_t AUDIO_BLOCK_SIZE = 8;
constexpr float CLIPPING_THRESHOLD = 0.95f;
constexpr uint32_t CLIPPING_BLINK_DURATION_TICKS = 100 * (48000 / AUDIO_BLOCK_SIZE) / 1000;
constexpr int DEBOUNCE_MS = 500;

class LedController {
public:
    LedController(Led& l) : led(l), baseBrightness(0.0f), currentBrightness(0.0f), blinksRemaining(0), isBlinking(false), blinkTicks(0), blinkDurationTicks(0), blinkState(false), keepOnAfter(false) {}

    void SetBaseBrightness(float brightness) {
        baseBrightness = brightness;
        if (!isBlinking) {
            currentBrightness = baseBrightness;
        }
    }

    void Blink(int times, bool keep_on = false, float brightness = 1.0f, int delay_ms = 250) {
        blinksRemaining = times;
        keepOnAfter = keep_on;
        blinkBrightness = brightness;
        // Audio rate = 48000 / AUDIO_BLOCK_SIZE
        blinkDurationTicks = delay_ms * (48000 / AUDIO_BLOCK_SIZE) / 1000;
        
        // Start with an off period of 200ms
        isBlinking = true;
        blinkState = false;
        blinkTicks = 200 * (48000 / AUDIO_BLOCK_SIZE) / 1000;
        currentBrightness = 0.0f;
    }

    void InterruptBlink(uint32_t offTickDuration) {
        isBlinking = true;
        blinkState = false;
        blinkTicks = offTickDuration;
        currentBrightness = 0.0f;
        blinksRemaining = 0;
        keepOnAfter = (baseBrightness > 0.0f);
        blinkBrightness = baseBrightness;
        blinkDurationTicks = 0;
    }

    void ProcessAudioRate() {
        if (!isBlinking) {
            currentBrightness = baseBrightness;
        } else {
            if (blinkTicks > 0) {
                blinkTicks--;
            } else {
                if (blinkState) {
                    blinkState = false;
                    currentBrightness = 0.0f;
                    blinkTicks = blinkDurationTicks;
                } else {
                    if (blinksRemaining > 0) {
                        blinksRemaining--;
                        blinkState = true;
                        currentBrightness = blinkBrightness;
                        blinkTicks = blinkDurationTicks;
                    } else {
                        isBlinking = false;
                        if (keepOnAfter) {
                            baseBrightness = blinkBrightness;
                        } else {
                            baseBrightness = 0.0f;
                        }
                        currentBrightness = baseBrightness;
                    }
                }
            }
        }
        
        led.Set(currentBrightness);
        led.Update();
    }

private:
    Led& led;
    float baseBrightness;
    float currentBrightness;
    
    int blinksRemaining;
    bool isBlinking;
    uint32_t blinkTicks;
    uint32_t blinkDurationTicks;
    bool blinkState;
    float blinkBrightness;
    bool keepOnAfter;
};

void blinkLedBlocking(Led& led, int times, bool keep_on = false, float brightness = 1.0f, int delay_ms = 250) {
    led.Set(0.0f); led.Update(); hw.DelayMs(200);
    for (int i = 0; i < times; i++) {
        led.Set(brightness); led.Update(); hw.DelayMs(delay_ms);
        if (i < times - 1 || !keep_on) {
            led.Set(0.0f); led.Update(); hw.DelayMs(delay_ms);
        }
    }
}

// DSP
Svf bassFilter;
IrLoader<MAX_IR_LENGTH, AUDIO_BLOCK_SIZE> irLoader;

// State
volatile bool isLoadingIr = false;
volatile bool clippingDetected = false;

LedController redLedController(ledRed);
LedController blueLedController(ledBlue);

// Audio callback - processes audio samples in blocks
// Called at audio rate: 48kHz / 8 samples = 6kHz
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

    // // Pre-process: read mono input, apply bass boost/cut, check input clipping
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

    // // IR convolution (handles bypass internally)
    float firOut[AUDIO_BLOCK_SIZE];
    // temp - IR ProcessBlock is crashing
    for (size_t i = 0; i < size; i++) {
        firOut[i] = firIn[i];
    }
    // irLoader.ProcessBlock(firIn, firOut, size);

    // // Apply output level, check output clipping, write stereo output
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

    // Load initial IR so we have audio immediately
    int initialPosition = irSwitch.Value();
    if (initialPosition >= 0 && initialPosition < (int)ImpulseResponseData::IR_COUNT) {
        irLoader.loadIr(initialPosition);
    } else {
        irLoader.irBypass = true;
    }

    // Blink blue LED to indicate the initial IR position as audio starts
    blueLedController.Blink(initialPosition + 1, false, 0.5f);

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
            blueLedController.Blink(position + 1, false, 0.5f);

            // Load IR or set bypass for empty slot
            if (position >= 0 && position < (int)ImpulseResponseData::IR_COUNT) {
                irLoader.loadIr(position);
            } else {
                irLoader.irBypass = true;
            }

            isLoadingIr = false;
        }

        // LED1 (red): always on, blinks off briefly on clipping
        if (clippingDetected) {
            clippingDetected = false;
            // Interrupt whatever it's doing and keep it off for 100ms
            redLedController.InterruptBlink(CLIPPING_BLINK_DURATION_TICKS);
        } else {
            redLedController.SetBaseBrightness(1.0f);
        }

        hw.DelayMs(50);
    }
}
