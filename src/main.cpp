// MuleBox - Guitar Processing Unit
// A simple passthrough audio application for the Electrosmith Daisy Seed
// running on the Cleveland Audio Hothouse platform.
//
// This program passes input audio directly to output, serving as a
// foundation for building guitar effects processing.

#include "hothouse.h"
#include "daisysp.h"
#include "hid/parameter.h"
#include "ImpulseResponse/ImpulseResponse.h"
#include "ImpulseResponse/ir_data.h"

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
Parameter irSelectorParam;  // For resistor ladder IR selection


/**
 * Fixed constants
 */
constexpr int SETTINGS_VERSION = 2;  // Bumped for IR selection
constexpr float SAMPLE_RATE = 48000.0f;  // Audio sample rate in Hz
static const float BASS_BOOST_FREQ = 110.0f;  // Center frequency in Hz
static const float BASS_BOOST_Q = 0.7f;       // Q factor (bandwidth)
constexpr int MAX_IR_COUNT = 12;              // Maximum number of IRs supported

/**
 * DSP Globals
 */
Svf bassBoost;
ImpulseResponse ir;
int currentIrIndex = 0;  // Currently loaded IR

/**
 * Persistent storage of settings
 */ 

struct Settings {
    int version;
    int irIndex;  // Selected IR index (0-11)

    bool operator!=(const Settings& a) const {
        return !(
            a.version == version &&
            a.irIndex == irIndex
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

    // Load IR index and initialize the IR
    currentIrIndex = localSettings.irIndex;

    // Clamp to valid range
    if (currentIrIndex < 0 || currentIrIndex >= (int)ImpulseResponseData::ir_collection.size()) {
        currentIrIndex = 0;
    }

    // Initialize IR with selected data
    ir.Init(ImpulseResponseData::ir_collection[currentIrIndex]);
}

void saveSettings() {
    //Reference to local copy of settings stored in flash
    Settings &localSettings = savedSettings.GetSettings();

    localSettings.version = SETTINGS_VERSION;
    localSettings.irIndex = currentIrIndex;

    triggerSettingsSave = true;
}






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
        float boosted = monoInput + (peakOutput * wetGain);

        // Process through impulse response (cabinet simulation)
        float irOutput = ir.Process(boosted);

        // Output to both stereo channels (dual mono)
        out[0][i] = irOutput;  // Left channel
        out[1][i] = irOutput;  // Right channel
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

    // Initialize IR selector (resistor ladder on KNOB_2)
    irSelectorParam.Init(hw.knobs[Hothouse::KNOB_2],
                         0.0f,     // Min value
                         11.0f,    // Max value (12 positions: 0-11)
                         Parameter::LINEAR);

    // Initialize bass boost EQ
    bassBoost.Init(hw.AudioSampleRate());  // Initialize with actual sample rate
    bassBoost.SetFreq(BASS_BOOST_FREQ);    // Center frequency
    bassBoost.SetRes(BASS_BOOST_Q);        // Q factor for musical width
    

    // Update settings
    Settings defaultSettings = {
        SETTINGS_VERSION, // version
        0                  // irIndex (default to first IR)
    };
    savedSettings.Init(defaultSettings);
    loadSettings();  // Load saved settings and initialize IR

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

        // Check IR selection from resistor ladder (KNOB_2)
        // Resistor ladder provides 12 discrete voltage levels
        float rawValue = irSelectorParam.Process();
        int selectedIrIndex = (int)(rawValue + 0.5f);  // Round to nearest integer

        // Clamp to valid range
        if (selectedIrIndex < 0) selectedIrIndex = 0;
        if (selectedIrIndex >= (int)ImpulseResponseData::ir_collection.size()) {
            selectedIrIndex = ImpulseResponseData::ir_collection.size() - 1;
        }

        // If IR selection changed, switch to new IR
        if (selectedIrIndex != currentIrIndex) {
            currentIrIndex = selectedIrIndex;
            ir.Init(ImpulseResponseData::ir_collection[currentIrIndex]);
            saveSettings();  // Persist the new selection
        }

        // Check if footswitch 1 is held for reset to bootloader mode
        hw.CheckResetToBootloader();

        // Small delay to prevent busy-waiting
        hw.DelayMs(10);
    }
}
