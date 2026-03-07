#pragma once

#include "daisysp.h"
#include "Filters/fir.h"
#include "ir_data.h"

template <size_t MaxLength, size_t BlockSize>
class IrLoader {
public:
    daisysp::FIR<MaxLength, BlockSize> firFilter;
    int currentIrIndex = 0;
    volatile bool irBypass = false;

    /**
     * Load IR from QSPI flash into FIR filter
     *
     * Sets irBypass=true during reconfiguration for thread safety
     * (audio callback checks irBypass before calling ProcessBlock).
     *
     * The FIR SetIR() reads coefficients directly from QSPI memory-mapped
     * addresses, reverses them internally, and stores in its own buffer.
     */
    void loadIr(int irIndex) {
        using namespace ImpulseResponseData;

        if (IR_COUNT == 0) {
            irBypass = true;
            return;
        }

        // Validate index
        if (irIndex < 0 || irIndex >= (int)IR_COUNT) {
            irIndex = 0;
        }

        // Bypass during reconfiguration to avoid audio glitches
        irBypass = true;

        const IRInfo& irInfo = ir_collection[irIndex];

        // Load IR into FIR filter (reverse=true: IR is time-forward, FIR needs time-reversed)
        firFilter.SetIR(irInfo.data, irInfo.length, true);

        currentIrIndex = irIndex;
        irBypass = false;
    }
    
    void ProcessBlock(float* in, float* out, size_t size) {
        if (!irBypass) {
            firFilter.ProcessBlock(in, out, size);
        } else {
            for (size_t i = 0; i < size; i++) {
                out[i] = in[i];
            }
        }
    }
};
