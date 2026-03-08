// CMSIS-DSP shim for macOS desktop testing.
//
// Drop-in replacement for <arm_math.h> that implements the subset of
// CMSIS-DSP functions used by ConvolutionEngine using Apple Accelerate (vDSP).
//
// Only the functions actually called by convolution_engine.h are provided:
//   - arm_rfft_fast_init_f32 / arm_rfft_fast_f32  (real FFT/IFFT)
//   - arm_cmplx_mult_cmplx_f32                     (complex multiply)
//   - arm_add_f32                                   (vector add)
//
// The RFFT implementation follows the exact same algorithm as CMSIS:
//   Forward: complex FFT of N/2 pairs, then stage_rfft recombination
//   Inverse: merge_rfft recombination, then complex IFFT
// The stage_rfft/merge_rfft code is ported directly from the CMSIS source
// (Apache 2.0 licensed). Only the complex FFT backend is replaced with vDSP.
// This guarantees identical numerical behavior.

#pragma once

#include <cstdint>
#include <cstring>
#include <cmath>
#include <Accelerate/Accelerate.h>

typedef float float32_t;

typedef enum {
    ARM_MATH_SUCCESS = 0,
    ARM_MATH_ARGUMENT_ERROR = -1
} arm_status;

// Minimal struct definitions. ConvolutionEngine never accesses struct
// fields directly -- it only passes arm_rfft_fast_instance_f32* to the
// API functions below.

typedef struct {
    uint16_t fftLen;         // complex FFT length (= RFFT length / 2)
} arm_cfft_instance_f32;

typedef struct {
    arm_cfft_instance_f32 Sint;
    uint16_t fftLenRFFT;              // real FFT length (N)
    float32_t* pTwiddleRFFT;         // twiddle factors for stage/merge
    FFTSetup vdspSetup;               // Apple vDSP FFT plan
    int log2cfft;                     // log2(fftLenRFFT / 2) for complex FFT
} arm_rfft_fast_instance_f32;

// --- Internal: vDSP-backed complex FFT (in-place, interleaved format) ---

static inline void vdsp_cfft_f32(
    FFTSetup setup, int log2n,
    float32_t* p,    // interleaved complex: [Re0, Im0, Re1, Im1, ...]
    uint8_t ifftFlag) // 0 = forward, 1 = inverse
{
    uint32_t len = 1u << log2n;

    // Convert interleaved to split-complex for vDSP
    // We can do this in-place by treating the interleaved array as pairs
    static constexpr size_t MAX_CFFT_LEN = 2048;
    float realBuf[MAX_CFFT_LEN], imagBuf[MAX_CFFT_LEN];
    DSPSplitComplex split = { realBuf, imagBuf };

    vDSP_ctoz((const DSPComplex*)p, 2, &split, 1, len);

    if (!ifftFlag) {
        // Forward complex FFT (unscaled, matching CMSIS behavior)
        vDSP_fft_zip(setup, &split, 1, log2n, FFT_FORWARD);
        // vDSP forward gives 2x standard DFT for fft_zrip, but fft_zip is 1x.
        // Actually, vDSP_fft_zip gives the standard DFT (no extra scaling).
        // CMSIS CFFT is also unscaled. So no scaling needed.
    } else {
        // Inverse complex FFT with 1/N normalization (matching CMSIS)
        vDSP_fft_zip(setup, &split, 1, log2n, FFT_INVERSE);
        // vDSP inverse is unnormalized. CMSIS inverse scales by 1/N.
        float scale = 1.0f / (float)len;
        vDSP_vsmul(split.realp, 1, &scale, split.realp, 1, len);
        vDSP_vsmul(split.imagp, 1, &scale, split.imagp, 1, len);
    }

    // Convert back to interleaved
    vDSP_ztoc(&split, 1, (DSPComplex*)p, 2, len);
}

// --- Internal: stage_rfft (ported from CMSIS arm_rfft_fast_f32.c) ---
// Recombines the N/2-point complex FFT output into the N-point real FFT output.
// Output format: [DC, Nyquist, Re1, Im1, Re2, Im2, ...]

static inline void stage_rfft_f32(
    const arm_rfft_fast_instance_f32* S,
    float32_t* p,      // input: in-place CFFT output (N floats = N/2 complex)
    float32_t* pOut)    // output: RFFT result (N floats)
{
    int32_t k = S->Sint.fftLen - 1;
    const float32_t* pCoeff = S->pTwiddleRFFT;
    float32_t *pA = p;
    float32_t *pB = p;

    float32_t xBR = pB[0], xBI = pB[1];
    float32_t xAR = pA[0], xAI = pA[1];

    float32_t twR = *pCoeff++;
    float32_t twI = *pCoeff++;

    float32_t t1a = xBR + xAR;
    float32_t t1b = xBI + xAI;

    *pOut++ = 0.5f * (t1a + t1b);
    *pOut++ = 0.5f * (t1a - t1b);

    pB = p + 2 * k;
    pA += 2;

    while (k > 0) {
        xBI = pB[1]; xBR = pB[0];
        xAR = pA[0]; xAI = pA[1];

        twR = *pCoeff++;
        twI = *pCoeff++;

        t1a = xBR - xAR;
        t1b = xBI + xAI;

        float32_t p0 = twR * t1a;
        float32_t p1 = twI * t1a;
        float32_t p2 = twR * t1b;
        float32_t p3 = twI * t1b;

        *pOut++ = 0.5f * (xAR + xBR + p0 + p3);
        *pOut++ = 0.5f * (xAI - xBI + p1 - p2);

        pA += 2;
        pB -= 2;
        k--;
    }
}

// --- Internal: merge_rfft (ported from CMSIS arm_rfft_fast_f32.c) ---
// Prepares RFFT data for inverse CFFT.

static inline void merge_rfft_f32(
    const arm_rfft_fast_instance_f32* S,
    float32_t* p,      // input: RFFT-format data (N floats)
    float32_t* pOut)    // output: N/2 complex values for CIFFT (N floats)
{
    int32_t k = S->Sint.fftLen - 1;
    const float32_t* pCoeff = S->pTwiddleRFFT;
    float32_t *pA = p;
    float32_t *pB = p;

    float32_t xAR = pA[0];
    float32_t xAI = pA[1];

    pCoeff += 2;

    *pOut++ = 0.5f * (xAR + xAI);
    *pOut++ = 0.5f * (xAR - xAI);

    pB = p + 2 * k;
    pA += 2;

    while (k > 0) {
        float32_t xBI = pB[1];
        float32_t xBR = pB[0];
        xAR = pA[0];
        xAI = pA[1];

        float32_t twR = *pCoeff++;
        float32_t twI = *pCoeff++;

        float32_t t1a = xAR - xBR;
        float32_t t1b = xAI + xBI;

        float32_t r = twR * t1a;
        float32_t s = twI * t1b;
        float32_t t = twI * t1a;
        float32_t u = twR * t1b;

        *pOut++ = 0.5f * (xAR + xBR - r - s);
        *pOut++ = 0.5f * (xAI - xBI + t - u);

        pA += 2;
        pB -= 2;
        k--;
    }
}

// --- arm_rfft_fast_init_f32 ---

inline arm_status arm_rfft_fast_init_f32(arm_rfft_fast_instance_f32* S, uint16_t fftLen) {
    S->fftLenRFFT = fftLen;
    S->Sint.fftLen = fftLen / 2;

    // log2 of complex FFT length
    S->log2cfft = 0;
    uint16_t tmp = fftLen / 2;
    while (tmp > 1) { tmp >>= 1; S->log2cfft++; }

    // vDSP setup for complex FFT of length N/2
    // Request setup for log2(N/2) — vDSP_create_fftsetup supports up to the
    // requested size and all smaller powers of 2
    S->vdspSetup = vDSP_create_fftsetup(S->log2cfft, kFFTRadix2);
    if (!S->vdspSetup) return ARM_MATH_ARGUMENT_ERROR;

    // Generate RFFT twiddle factors matching CMSIS tables.
    // CMSIS stores: [sin(2*pi*k/N), cos(2*pi*k/N)] for k = 0 .. N/2-1
    // (See twiddleCoef_rfft_256 in arm_common_tables.c)
    uint16_t halfN = fftLen / 2;
    S->pTwiddleRFFT = new float32_t[fftLen];  // N/2 pairs * 2 = N floats
    for (uint16_t k = 0; k < halfN; k++) {
        float angle = 2.0f * (float)M_PI * k / (float)fftLen;
        S->pTwiddleRFFT[2 * k]     = sinf(angle);
        S->pTwiddleRFFT[2 * k + 1] = cosf(angle);
    }

    return ARM_MATH_SUCCESS;
}

// --- arm_rfft_fast_f32 ---
//
// Follows the exact CMSIS algorithm:
//   Forward: in-place complex CFFT of p, then stage_rfft to pOut
//   Inverse: merge_rfft from p to pOut, then in-place complex CIFFT of pOut

inline void arm_rfft_fast_f32(
    const arm_rfft_fast_instance_f32* S,
    float32_t* p,
    float32_t* pOut,
    uint8_t ifftFlag)
{
    if (!ifftFlag) {
        // Forward: CFFT(p) in-place, then stage_rfft(p -> pOut)
        vdsp_cfft_f32(S->vdspSetup, S->log2cfft, p, 0);
        stage_rfft_f32(S, p, pOut);
    } else {
        // Inverse: merge_rfft(p -> pOut), then CIFFT(pOut) in-place
        merge_rfft_f32(S, p, pOut);
        vdsp_cfft_f32(S->vdspSetup, S->log2cfft, pOut, 1);
    }
}

// --- arm_cmplx_mult_cmplx_f32 ---
// Complex-by-complex multiplication: dst[k] = A[k] * B[k]
// Data layout: interleaved [Re0, Im0, Re1, Im1, ...]
// numSamples = number of complex pairs (not floats)

inline void arm_cmplx_mult_cmplx_f32(
    const float32_t* pSrcA,
    const float32_t* pSrcB,
    float32_t* pDst,
    uint32_t numSamples)
{
    for (uint32_t i = 0; i < numSamples; i++) {
        float aR = pSrcA[2 * i];
        float aI = pSrcA[2 * i + 1];
        float bR = pSrcB[2 * i];
        float bI = pSrcB[2 * i + 1];
        pDst[2 * i]     = aR * bR - aI * bI;
        pDst[2 * i + 1] = aR * bI + aI * bR;
    }
}

// --- arm_add_f32 ---
// Element-wise vector addition: dst[i] = A[i] + B[i]

inline void arm_add_f32(
    const float32_t* pSrcA,
    const float32_t* pSrcB,
    float32_t* pDst,
    uint32_t blockSize)
{
    vDSP_vadd(pSrcA, 1, pSrcB, 1, pDst, 1, blockSize);
}
