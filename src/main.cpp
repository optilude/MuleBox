// MuleBox - Guitar Processing Unit
// A simple passthrough audio application for the Electrosmith Daisy Seed
// running on the Cleveland Audio Hothouse platform.
//
// This program passes input audio directly to output, serving as a
// foundation for building guitar effects processing.

#include "hothouse.h"

using clevelandmusicco::Hothouse;

// Hardware interface
Hothouse hw;

// Audio callback - processes audio samples
// This is called at the audio rate (typically 48kHz / block size)
void AudioCallback(AudioHandle::InputBuffer in,
                   AudioHandle::OutputBuffer out,
                   size_t size) {
    // Simple passthrough: copy input to output
    // in[0] and out[0] are left channel
    // in[1] and out[1] are right channel
    for (size_t i = 0; i < size; i++) {
        out[0][i] = in[0][i];  // Left channel
        out[1][i] = in[1][i];  // Right channel
    }
}

int main(void) {
    // Initialize the Hothouse hardware
    hw.Init();

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
