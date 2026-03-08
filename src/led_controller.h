#pragma once

#include "daisy.h"

class LedController {
public:
    LedController(daisy::Led& l, uint32_t sample_rate, size_t block_size) 
        : led(l), sampleRate(sample_rate), blockSize(block_size),
          baseBrightness(0.0f), currentBrightness(0.0f), 
          blinksRemaining(0), isBlinking(false), blinkTicks(0), 
          blinkDurationTicks(0), blinkState(false), keepOnAfter(false) {}

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
        
        uint32_t audioRate = sampleRate / blockSize;
        blinkDurationTicks = delay_ms * audioRate / 1000;
        
        // Start with an off period of 200ms
        isBlinking = true;
        blinkState = false;
        blinkTicks = 200 * audioRate / 1000;
        currentBrightness = 0.0f;
    }

    void InterruptBlink(uint32_t offTickDuration, float tempBrightness = 0.0f) {
        isBlinking = true;
        blinkState = false;
        blinkTicks = offTickDuration;
        currentBrightness = tempBrightness;
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
    daisy::Led& led;
    uint32_t sampleRate;
    size_t blockSize;
    
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