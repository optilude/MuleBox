#include "daisysp.h"
#include "hothouse.h"
#include "debounced_analog_switch.h"
#include "ir_loader.h"
#include "ir_data.h"
#include "stlink_print.h"

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
IrLoader<MAX_IR_LENGTH, AUDIO_BLOCK_SIZE> irLoader;

constexpr int MAX_IR_POSITIONS = 12;
constexpr int DEBOUNCE_MS = 500;

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out,
                   size_t size) {
    
    // This could be in main() but seems to work most reliably if it's here
    irSwitch.Process();

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
            irSwitch.ResetChangedFlag();
            blinkLed(ledBlue, irSwitch.Value() + 1, true);
        }
        hw.DelayMs(100);
    }
    return 0;
}
