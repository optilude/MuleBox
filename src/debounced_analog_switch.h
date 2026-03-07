#pragma once

#include "daisy_core.h"
#include "daisy_seed.h"

using daisy::AnalogControl;
using daisy::Parameter;

// Helper class to debounce discrete selections from analog controls
class DebouncedAnalogSwitch {
  public:
    void Init(AnalogControl knob, uint32_t positions, uint32_t debounceMs) {
        param_.Init(knob, 0.0f, (float)(positions - 1), Parameter::LINEAR);

        debounceMs_ = debounceMs;
        
        lastChangeTime_ = 0;
        stableValue_ = -1;
        pendingValue_ = -1;

        hasChanged = false;
    }

    int Process() {
        int rawValue = (int)(param_.Process() + 0.5f); // Round to nearest integer
        uint32_t now = daisy::System::GetNow();

        // First run initialization
        if (stableValue_ == -1) {
            stableValue_ = rawValue;
            pendingValue_ = -1;
            hasChanged = true;
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
                    hasChanged = true;
                }
            }
        } else {
            // We are back at the stable position
            pendingValue_ = -1;
        }
        return stableValue_;
    }

    int Value() const { return stableValue_; }
    bool HasChanged() const { return hasChanged; }
    void ResetChangedFlag() { hasChanged = false; }

  private:
    daisy::Parameter param_;
    uint32_t debounceMs_;
    uint32_t lastChangeTime_;
    int stableValue_;
    int pendingValue_;
    bool hasChanged;
};
