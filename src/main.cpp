// MuleBox - Guitar Processing Unit
// A simple passthrough audio application for the Electrosmith Daisy Seed
// running on the Cleveland Audio Hothouse platform.
//
// This program passes input audio directly to output, serving as a
// foundation for building guitar effects processing.

#include "hothouse.h"
#include "daisysp.h"
#include "hid/parameter.h"

using clevelandmusicco::Hothouse;
using namespace daisysp;
using daisy::Parameter;

// Hardware interface
Hothouse hw;

// Bass boost EQ filter
Svf bassBoost;

// Bass boost parameters
static const float BASS_BOOST_FREQ = 110.0f;  // Center frequency in Hz
static const float BASS_BOOST_Q = 0.7f;       // Q factor (bandwidth)

// Parameter mapping for boost gain control
Parameter boostGainParam;

// Audio callback - processes audio samples
// This is called at the audio rate (typically 48kHz / block size)
void AudioCallback(AudioHandle::InputBuffer in,
                   AudioHandle::OutputBuffer out,
                   size_t size) {
    // Process boost gain parameter (maps knob to 0-3.0x range)
    float wetGain = boostGainParam.Process();

    for (size_t i = 0; i < size; i++) {
        // Read mono input from left channel only
        float monoInput = in[0][i];

        // Process through SVF bass boost filter
        bassBoost.Process(monoInput);
        float peakOutput = bassBoost.Peak();

        // Blend dry signal with boosted peak output
        float processed = monoInput + (peakOutput * wetGain);

        // Output to both stereo channels (dual mono)
        out[0][i] = processed;  // Left channel
        out[1][i] = processed;  // Right channel
    }
}

int main(void) {
    // Initialize the Hothouse hardware
    hw.Init();

    // Initialize bass boost EQ
    bassBoost.Init(hw.AudioSampleRate());  // Initialize with actual sample rate
    bassBoost.SetFreq(BASS_BOOST_FREQ);    // Center frequency
    bassBoost.SetRes(BASS_BOOST_Q);        // Q factor for musical width

    // Initialize boost gain parameter (0.0 to 3.0x for ~0-12dB boost)
    // Using LOGARITHMIC curve for more musical response (finer control at lower levels)
    boostGainParam.Init(hw.knobs[Hothouse::KNOB_1],
                        0.0f,      // Min: no boost
                        3.0f,      // Max: ~12dB boost (10^(12/20) ≈ 3.98)
                        Parameter::LOGARITHMIC);

    // Set audio parameters
    hw.SetAudioBlockSize(48);  // Process 48 samples at a time

    // Start audio processing with our callback
    hw.StartAudio(AudioCallback);

    // Main loop - runs continuously
    while(1) {
        // Process all hardware controls (knobs, switches)
        hw.ProcessAllControls();

        // Check if footswitch 1 is held for reset to bootloader mode
        hw.CheckResetToBootloader();

        // Small delay to prevent busy-waiting
        hw.DelayMs(10);
    }
}
