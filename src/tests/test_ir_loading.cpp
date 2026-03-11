// Simple test of rotary knob blinking and IR loading (but not processing!)
// Red LED blinks on startup to indicate number of IRs loaded
// Blue LED blinks to indicate position of IR knob when changed
// LED stays on if IR loaded, off if not (empty slot or fault)

#include "daisysp.h"
#include "hothouse.h"
#include "debounced_analog_switch.h"
#include "ir_loader.h"
#include "ir_data.h"
#include "stlink_print.h"

#include "dev/sdram.h"

using clevelandmusicco::Hothouse;
using daisy::AudioHandle;
using daisy::Led;
using daisy::Parameter;
using daisy::SaiHandle;

Hothouse hw;

Led ledRed, ledBlue;
DebouncedAnalogSwitch irSwitch;

constexpr size_t MAX_IR_LENGTH = 8192;
constexpr size_t AUDIO_BLOCK_SIZE = 8;
constexpr size_t MAX_PARTITIONS = MAX_IR_LENGTH / AUDIO_BLOCK_SIZE;

float DSY_SDRAM_BSS irFreqBuf[MAX_PARTITIONS * 2 * AUDIO_BLOCK_SIZE];
float DSY_SDRAM_BSS fdlBuf[MAX_PARTITIONS * 2 * AUDIO_BLOCK_SIZE];

IrLoader irLoader;

constexpr int MAX_IR_POSITIONS = 12;
constexpr int DEBOUNCE_MS = 500;

bool isLoadingIr = false;

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out,
                   size_t size) {
    
    // This could be in main() but seems to work most reliably if it's here
    // We wait if we are in the middle of loading an IR to avoid thread safety issues
    // In the main() loop, we check for irSwitch.HasChanged() and then act on that
    if (!isLoadingIr) {
        irSwitch.Process();
        
        ledBlue.Set(irLoader.irBypass ? 0.0f : 0.75f);
        ledBlue.Update();
    }


    // Audio pass-through
    for (size_t i = 0; i < size; ++i) {
        out[0][i] = out[1][i] = in[0][i];
    }
}

void blinkLed(Led& led, int times, bool keep_on = false, int delay_ms = 250) {
    led.Set(0.0f); led.Update(); hw.DelayMs(200);
    for (int i = 0; i < times; i++) {
        led.Set(1.0f); led.Update(); hw.DelayMs(delay_ms);
        led.Set(0.0f); led.Update(); hw.DelayMs(delay_ms);
    }
    if(keep_on) {
        led.Set(1.0f); led.Update();
    }
}

int main() {
    hw.Init(true);
    hw.SetAudioBlockSize(AUDIO_BLOCK_SIZE);
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);

    irLoader.Init(MAX_PARTITIONS, irFreqBuf, fdlBuf);

    irSwitch.Init(hw.knobs[Hothouse::KNOB_6], MAX_IR_POSITIONS, DEBOUNCE_MS);

    // Initialize LEDs
    ledRed.Init(hw.seed.GetPin(Hothouse::LED_1), false);
    ledBlue.Init(hw.seed.GetPin(Hothouse::LED_2), false);

    hw.StartAdc();
    
    // 1. Blink red LED for each loaded IR on startup and keep on
    blinkLed(ledRed, ImpulseResponseData::IR_COUNT - 1, true);

    // Stabilise the rotary switch at startup
    for (int i = 0; i < 10; i++) {
        irSwitch.Process();
        hw.DelayMs(100);
    }

    hw.StartAudio(AudioCallback);

    while (true) {
        
        // Check for changes in the IR selector switch
        if(irSwitch.HasChanged()) {
            isLoadingIr = true;
            irSwitch.ResetChangedFlag();
            
            int position = irSwitch.Value();
            blinkLed(ledBlue, position + 1, false);

            // Check if there is an IR at the position, and if so set blue LED to on
            if (position >= 0 && position < (int)ImpulseResponseData::IR_COUNT) {
                irLoader.loadIr(position);
            } else {
                irLoader.irBypass = true;
            }

            isLoadingIr = false;
        }
        hw.DelayMs(100);
    }
    return 0;
}
