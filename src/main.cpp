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
constexpr int SETTINGS_VERSION = 4;  // Bumped for IR bypass flag removal
constexpr float SAMPLE_RATE = 48000.0f;  // Audio sample rate in Hz
static const float BASS_BOOST_FREQ = 110.0f;  // Center frequency in Hz
static const float BASS_BOOST_Q = 0.7f;       // Q factor (bandwidth)
constexpr int MAX_IR_POSITIONS = 12;          // Rotary positions supported by hardware

/**
 * DSP Globals
 */
Svf bassBoost;
ImpulseResponse ir;
int currentIrIndex = 0;  // Currently loaded IR
volatile bool irBypass = false;  // When true, skip IR convolution

// RAM buffer for active IR (copied from QSPI flash)
// Max size is 8,192 samples as defined in ImpulseResponse
constexpr size_t MAX_IR_BUFFER_SIZE = 8192;
static float irRamBuffer[MAX_IR_BUFFER_SIZE];

// Load IR from QSPI flash to RAM buffer
void loadIrToRam(int irIndex) {
    using namespace ImpulseResponseData;

    // If no IRs are compiled in, nothing to load.
    if (IR_COUNT == 0) {
        irBypass = true;
        return;
    }

    // Validate index
    if (irIndex < 0 || irIndex >= (int)IR_COUNT) {
        irIndex = 0;
    }

    const IRInfo& irInfo = ir_collection[irIndex];

    // Copy from QSPI to RAM
    // Note: memcpy would be faster but this is safer for QSPI access
    for (size_t i = 0; i < irInfo.length; i++) {
        irRamBuffer[i] = irInfo.data[i];
    }

    // Initialize IR processor with RAM buffer
    ir.Init(irRamBuffer, irInfo.length);
    currentIrIndex = irIndex;
    irBypass = false;
}

/**
 * Persistent storage of settings
 */ 

struct Settings {
    int version;
    int irIndex;   // Last selected IR index (0-11)

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

    // Load IR index and copy from QSPI to RAM
    int irIndex = localSettings.irIndex;

    // If no IRs exist in this build, force bypass.
    if (ImpulseResponseData::IR_COUNT == 0) {
        irBypass = true;
        return;
    }

    // Clamp to valid range to ensure safety if IR list changed
    if (irIndex < 0 || irIndex >= (int)ImpulseResponseData::IR_COUNT) {
        irIndex = 0;
    }

    // Load IR from QSPI flash to RAM and initialize processor
    // This will also un-bypass
    loadIrToRam(irIndex);
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

        // If selector points beyond available IRs, bypass convolution.
        float output = boosted;
        if (!irBypass) {
            output = ir.Process(boosted);
        }

        // Output to both stereo channels (dual mono)
        out[0][i] = output;  // Left channel
        out[1][i] = output;  // Right channel
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
                         (float)(MAX_IR_POSITIONS - 1),    // Max value (12 positions: 0-11)
                         Parameter::LINEAR);

    // Initialize bass boost EQ
    bassBoost.Init(hw.AudioSampleRate());  // Initialize with actual sample rate
    bassBoost.SetFreq(BASS_BOOST_FREQ);    // Center frequency
    bassBoost.SetRes(BASS_BOOST_Q);        // Q factor for musical width
    

    // Update settings
    Settings defaultSettings = {
        SETTINGS_VERSION, // version
        0                 // irIndex (default to first IR)
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
        int selectedPosition = (int)(rawValue + 0.5f);  // Round to nearest integer

        // Clamp to selector's physical range (0..11)
        if (selectedPosition < 0) selectedPosition = 0;
        if (selectedPosition >= MAX_IR_POSITIONS) selectedPosition = MAX_IR_POSITIONS - 1;

        // Bypass if selector position exceeds compiled IR count.
        bool shouldBypass = (selectedPosition >= (int)ImpulseResponseData::IR_COUNT);

        // Apply selection changes
        if (shouldBypass != irBypass) {
            irBypass = shouldBypass;
            saveSettings();
        }

        // If not bypassed and IR selection changed, load new IR from QSPI to RAM
        if (!shouldBypass && selectedPosition != currentIrIndex) {
            loadIrToRam(selectedPosition);
            saveSettings();  // Persist the new selection
        }

        // Check if footswitch 1 is held for reset to bootloader mode
        hw.CheckResetToBootloader();

        // Small delay to prevent busy-waiting
        hw.DelayMs(10);
    }
}
