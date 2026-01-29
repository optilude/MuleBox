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

using daisy::Parameter;
using daisy::Led;
using daisy::PersistentStorage;
using daisy::SaiHandle;
using daisy::AudioHandle;

using daisysp::Svf;

/**
 * Hardware interface
 */

Hothouse hw;
Led ledLeft, ledRight;
Parameter boostGainParam;


/**
 * Fixed constants
 */
constexpr int SETTINGS_VERSION = 1;
constexpr float SAMPLE_RATE = 48000.0f;  // Audio sample rate in Hz
static const float BASS_BOOST_FREQ = 110.0f;  // Center frequency in Hz
static const float BASS_BOOST_Q = 0.7f;       // Q factor (bandwidth)

/**
 * Persistent storage of settings
 */ 

struct Settings {
    int version;
  
    bool operator!=(const Settings& a) const {
        return !(
            a.version == version
        );
    }
};

PersistentStorage<Settings> savedSettings(hw.seed.qspi);
bool triggerSettingsSave = false;

void loadSettings() {

    // Reference to local copy of settings stored in flash
    Settings &localSettings = savedSettings.GetSettings();

    int savedVersion = localSettings.version;

    if (savedVersion != SETTINGS_VERSION) {
        // Something has changed. Load defaults!
        savedSettings.RestoreDefaults();
        loadSettings();
        return;
    }
}

void saveSettings() {
    //Reference to local copy of settings stored in flash
    Settings &localSettings = savedSettings.GetSettings();

    localSettings.version = SETTINGS_VERSION;

    triggerSettingsSave = true;
}



/**
 * Globals
 */
Svf bassBoost;



// Audio callback - processes audio samples
// This is called at the audio rate (typically 48kHz / block size)
void AudioCallback(AudioHandle::InputBuffer in,
                   AudioHandle::OutputBuffer out,
                   size_t size) {
    
    ledLeft.Update();
    ledRight.Update();

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
    hw.Init(true); // max CPU speed
    hw.SetAudioBlockSize(8);  // Process 8 samples at a time
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);

    ledLeft.Init(hw.seed.GetPin(Hothouse::LED_1), false);
    ledRight.Init(hw.seed.GetPin(Hothouse::LED_2), false);

    boostGainParam.Init(hw.knobs[Hothouse::KNOB_1],
                        0.0f,      // Min: no boost
                        3.0f,      // Max: ~12dB boost (10^(12/20) ≈ 3.98)
                        Parameter::LOGARITHMIC);

    // Initialize bass boost EQ
    bassBoost.Init(hw.AudioSampleRate());  // Initialize with actual sample rate
    bassBoost.SetFreq(BASS_BOOST_FREQ);    // Center frequency
    bassBoost.SetRes(BASS_BOOST_Q);        // Q factor for musical width
    

    // Update settings
    Settings defaultSettings = {
        SETTINGS_VERSION // version
    };
    savedSettings.Init(defaultSettings);

    // Start audio processing with our callback
    hw.StartAdc();
    hw.StartAudio(AudioCallback);

    // Main loop - runs continuously
    while(1) {

        // Save settings if triggered
        if(triggerSettingsSave) {
            savedSettings.Save();
            triggerSettingsSave = false;
        }

        // Process all hardware controls (knobs, switches)
        hw.ProcessAllControls();

        // Check if footswitch 1 is held for reset to bootloader mode
        hw.CheckResetToBootloader();

        // Small delay to prevent busy-waiting
        hw.DelayMs(10);
    }
}
