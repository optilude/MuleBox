// Simple test of knobs and LEDs for Mulebox
// The bass knob (knob 5) controls the red LED brightness
// The level knob (knob 4) controls the blue LED brightness

#include "daisysp.h"
#include "hothouse.h"

using clevelandmusicco::Hothouse;
using daisy::AudioHandle;
using daisy::Led;
using daisy::Parameter;
using daisy::SaiHandle;

Hothouse hw;

// LEDs for bypass and switch example
Led ledRed, ledBlue;
Parameter knobBass, knobLevel;

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out,
                   size_t size) {
  hw.ProcessAllControls();

  // When lit, scale LED_1 brightness with KNOB_1 (assigned to parm_bright)
  ledRed.Set(knobBass.Process() * 1.0f);
  ledBlue.Set(knobLevel.Process() * 1.0f);

  // Update the LEDs
  ledRed.Update();
  ledBlue.Update();

  // Audio processing; since this is a HelloWorld,
  // just pass the input to the output in either state
  for (size_t i = 0; i < size; ++i) {
      out[0][i] = out[1][i] = in[0][i];
  }
}

int main() {
  hw.Init();
  hw.SetAudioBlockSize(4);  // Number of samples handled per callback
  hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);

  knobBass.Init(hw.knobs[Hothouse::KNOB_5], 0.3f, 1.0f, Parameter::LINEAR);
  knobLevel.Init(hw.knobs[Hothouse::KNOB_4], 0.3f, 1.0f, Parameter::LINEAR);

  // Initialize LEDs
  ledRed.Init(hw.seed.GetPin(Hothouse::LED_1), false);
  ledBlue.Init(hw.seed.GetPin(Hothouse::LED_2), false);

  hw.StartAdc();
  hw.StartAudio(AudioCallback);

  while (true) {
    hw.DelayMs(10);
  }
  return 0;
}