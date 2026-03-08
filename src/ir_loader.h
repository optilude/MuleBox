#pragma once

#include "convolution_engine.h"
#include "ir_data.h"

class IrLoader {
public:
    ConvolutionEngine convolution;
    int currentIrIndex = 0;
    volatile bool irBypass = false;

    // irFreqBuf and fdlBuf must be SDRAM-allocated arrays of
    // maxPartitions * ConvolutionEngine::N floats each.
    void Init(size_t maxPartitions, float* irFreqBuf, float* fdlBuf) {
        convolution.Init(maxPartitions, irFreqBuf, fdlBuf);
    }

    /**
     * Load IR into convolution engine.
     *
     * Sets irBypass=true during reconfiguration for thread safety
     * (audio callback checks irBypass before calling ProcessBlock).
     *
     * Prepare() pre-computes FFTs of IR partitions — takes a few ms,
     * so this should only be called from the main loop.
     */
    void loadIr(int irIndex) {
        using namespace ImpulseResponseData;

        if (IR_COUNT == 0) {
            irBypass = true;
            return;
        }

        if (irIndex < 0 || irIndex >= (int)IR_COUNT) {
            irIndex = 0;
        }

        irBypass = true;

        const IRInfo& irInfo = ir_collection[irIndex];
        convolution.Prepare(irInfo.data, irInfo.length);

        currentIrIndex = irIndex;
        irBypass = false;
    }

    void ProcessBlock(const float* in, float* out, size_t size) {
        if (!irBypass) {
            convolution.ProcessBlock(in, out, size);
        } else {
            memcpy(out, in, size * sizeof(float));
        }
    }
};
