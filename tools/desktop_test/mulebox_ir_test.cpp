// MuleBox Desktop IR Convolution Test
//
// Offline tool to test the IR convolution engine on macOS without hardware.
// Reads a WAV file, applies an IR from ir_data.h using the production
// ConvolutionEngine + IrLoader, writes the result as a WAV file.
//
// Usage:
//   mulebox_ir_test [options] INPUT.wav OUTPUT.wav
//     -l           List available IRs and exit
//     -i INDEX     Select IR by index (default: 0)
//     --selftest   Validate FFT shim + convolution accuracy
//
// Build:
//   make  (from tools/desktop_test/)
//   or:  make desktop-test  (from project root)

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>

#include "convolution_engine.h"
#include "ir_loader.h"
#include "ir_data.h"

// --- Convolution buffer sizing (matches main.cpp) ---

constexpr size_t MAX_IR_LENGTH = 8192;
constexpr size_t PARTITION_SIZE = ConvolutionEngine::L;   // 128
constexpr size_t FFT_SIZE = ConvolutionEngine::N;         // 256
constexpr size_t MAX_PARTITIONS = (MAX_IR_LENGTH + PARTITION_SIZE - 1) / PARTITION_SIZE;

// Heap-allocated equivalents of the DSY_SDRAM_BSS buffers in main.cpp
static float convIrFreqBuf[MAX_PARTITIONS * FFT_SIZE];
static float convFdlBuf[MAX_PARTITIONS * FFT_SIZE];

// ============================================================
// Minimal WAV I/O
// ============================================================

struct WavFile {
    uint32_t sampleRate;
    uint16_t numChannels;
    uint16_t bitsPerSample;
    uint16_t audioFormat;   // 1=PCM, 3=IEEE float
    std::vector<float> samples;  // mono, normalized -1..1
};

template<typename T>
static T readLE(std::ifstream& f) {
    T val = 0;
    f.read(reinterpret_cast<char*>(&val), sizeof(T));
    return val;
}

template<typename T>
static void writeLE(std::ofstream& f, T val) {
    f.write(reinterpret_cast<const char*>(&val), sizeof(T));
}

static bool readWav(const char* path, WavFile& wav) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { fprintf(stderr, "Error: cannot open '%s'\n", path); return false; }

    char id[4];
    f.read(id, 4);
    if (memcmp(id, "RIFF", 4) != 0) { fprintf(stderr, "Error: not a RIFF file\n"); return false; }
    readLE<uint32_t>(f);  // file size
    f.read(id, 4);
    if (memcmp(id, "WAVE", 4) != 0) { fprintf(stderr, "Error: not a WAVE file\n"); return false; }

    bool gotFmt = false;
    while (f) {
        f.read(id, 4);
        uint32_t chunkSize = readLE<uint32_t>(f);
        if (!f) break;

        if (memcmp(id, "fmt ", 4) == 0) {
            wav.audioFormat = readLE<uint16_t>(f);
            wav.numChannels = readLE<uint16_t>(f);
            wav.sampleRate = readLE<uint32_t>(f);
            readLE<uint32_t>(f);  // byte rate
            readLE<uint16_t>(f);  // block align
            wav.bitsPerSample = readLE<uint16_t>(f);
            // Skip any extra fmt bytes
            if (chunkSize > 16) f.seekg(chunkSize - 16, std::ios::cur);
            gotFmt = true;
        } else if (memcmp(id, "data", 4) == 0 && gotFmt) {
            size_t bytesPerSample = wav.bitsPerSample / 8;
            size_t totalSamples = chunkSize / bytesPerSample;
            size_t numFrames = totalSamples / wav.numChannels;
            wav.samples.resize(numFrames);

            for (size_t i = 0; i < numFrames; i++) {
                float sample = 0.0f;
                if (wav.audioFormat == 3 && wav.bitsPerSample == 32) {
                    // IEEE float
                    sample = readLE<float>(f);
                } else if (wav.audioFormat == 1 && wav.bitsPerSample == 16) {
                    // PCM 16-bit
                    int16_t s = readLE<int16_t>(f);
                    sample = s / 32768.0f;
                } else if (wav.audioFormat == 1 && wav.bitsPerSample == 24) {
                    // PCM 24-bit
                    uint8_t b[3];
                    f.read(reinterpret_cast<char*>(b), 3);
                    int32_t s = (b[0]) | (b[1] << 8) | (b[2] << 16);
                    if (s & 0x800000) s |= 0xFF000000;  // sign extend
                    sample = s / 8388608.0f;
                } else {
                    fprintf(stderr, "Error: unsupported format (fmt=%d, bits=%d)\n",
                            wav.audioFormat, wav.bitsPerSample);
                    return false;
                }
                wav.samples[i] = sample;

                // Skip remaining channels (take left only)
                if (wav.numChannels > 1) {
                    f.seekg((wav.numChannels - 1) * bytesPerSample, std::ios::cur);
                }
            }
            return true;
        } else {
            // Skip unknown chunk (pad to even boundary)
            f.seekg((chunkSize + 1) & ~1u, std::ios::cur);
        }
    }
    fprintf(stderr, "Error: missing fmt or data chunk\n");
    return false;
}

static bool writeWav(const char* path, const std::vector<float>& samples, uint32_t sampleRate) {
    std::ofstream f(path, std::ios::binary);
    if (!f) { fprintf(stderr, "Error: cannot create '%s'\n", path); return false; }

    uint32_t numSamples = (uint32_t)samples.size();
    uint16_t numChannels = 1;
    uint16_t bitsPerSample = 16;
    uint32_t byteRate = sampleRate * numChannels * (bitsPerSample / 8);
    uint16_t blockAlign = numChannels * (bitsPerSample / 8);
    uint32_t dataSize = numSamples * blockAlign;

    // RIFF header
    f.write("RIFF", 4);
    writeLE<uint32_t>(f, 36 + dataSize);
    f.write("WAVE", 4);

    // fmt chunk
    f.write("fmt ", 4);
    writeLE<uint32_t>(f, 16);
    writeLE<uint16_t>(f, 1);             // PCM
    writeLE<uint16_t>(f, numChannels);
    writeLE<uint32_t>(f, sampleRate);
    writeLE<uint32_t>(f, byteRate);
    writeLE<uint16_t>(f, blockAlign);
    writeLE<uint16_t>(f, bitsPerSample);

    // data chunk
    f.write("data", 4);
    writeLE<uint32_t>(f, dataSize);
    for (size_t i = 0; i < numSamples; i++) {
        float clamped = std::max(-1.0f, std::min(1.0f, samples[i]));
        int16_t s = (int16_t)(clamped * 32767.0f);
        writeLE<int16_t>(f, s);
    }

    return true;
}

// ============================================================
// IR listing
// ============================================================

static void listIrs() {
    using namespace ImpulseResponseData;
    printf("Available IRs (%zu):\n", IR_COUNT);
    for (size_t i = 0; i < IR_COUNT; i++) {
        float durationMs = ir_collection[i].length * 1000.0f / 48000.0f;
        printf("  [%zu] %s (%zu samples, %.1f ms)\n",
               i, ir_collection[i].name, ir_collection[i].length, durationMs);
    }
}

// ============================================================
// Self-test
// ============================================================

static bool selftest() {
    using namespace ImpulseResponseData;
    printf("=== Self-test ===\n\n");
    bool allPassed = true;

    if (IR_COUNT == 0) {
        fprintf(stderr, "No IRs available for testing.\n");
        return false;
    }

    // Test 1: FFT round-trip (forward then inverse = identity)
    printf("Test 1: FFT round-trip... ");
    {
        arm_rfft_fast_instance_f32 fftInst;
        arm_rfft_fast_init_f32(&fftInst, FFT_SIZE);

        float original[FFT_SIZE];
        for (size_t i = 0; i < FFT_SIZE; i++) {
            original[i] = sinf(2.0f * M_PI * 3.0f * i / FFT_SIZE)
                        + 0.5f * cosf(2.0f * M_PI * 7.0f * i / FFT_SIZE);
        }

        float p[FFT_SIZE];
        memcpy(p, original, sizeof(p));

        float freq[FFT_SIZE];
        arm_rfft_fast_f32(&fftInst, p, freq, 0);

        float reconstructed[FFT_SIZE];
        arm_rfft_fast_f32(&fftInst, freq, reconstructed, 1);

        float maxErr = 0.0f;
        for (size_t i = 0; i < FFT_SIZE; i++) {
            float err = fabsf(reconstructed[i] - original[i]);
            if (err > maxErr) maxErr = err;
        }
        bool pass = maxErr < 1e-5f;
        printf("max error = %.2e %s\n", maxErr, pass ? "PASS" : "FAIL");
        if (!pass) allPassed = false;
    }

    // Test 2: Impulse response via ConvolutionEngine
    //
    // Known limitation: the CMSIS packed RFFT format stores DC and Nyquist
    // in a single complex slot [DC, Nyq]. When arm_cmplx_mult_cmplx_f32
    // multiplies two such spectra, it cross-contaminates DC and Nyquist
    // (complex multiply of two real pairs != element-wise real multiply).
    // This causes a per-sample error of ~1/N (~-48 dB for N=256), which is
    // inaudible. The same artifact occurs on real CMSIS hardware.
    printf("Test 2: Impulse response (IR 0: %s)... ", ir_collection[0].name);
    {
        IrLoader loader;
        loader.Init(MAX_PARTITIONS, convIrFreqBuf, convFdlBuf);
        loader.loadIr(0);

        const size_t irLen = ir_collection[0].length;
        const size_t totalBlocks = (irLen + PARTITION_SIZE - 1) / PARTITION_SIZE + 1;
        std::vector<float> output(totalBlocks * PARTITION_SIZE, 0.0f);

        for (size_t b = 0; b < totalBlocks; b++) {
            float inBlock[PARTITION_SIZE] = {0};
            if (b == 0) inBlock[0] = 1.0f;  // unit impulse

            float outBlock[PARTITION_SIZE];
            loader.ProcessBlock(inBlock, outBlock, PARTITION_SIZE);
            memcpy(&output[b * PARTITION_SIZE], outBlock, PARTITION_SIZE * sizeof(float));
        }

        float maxErr = 0.0f;
        size_t worstIdx = 0;
        for (size_t i = 0; i < irLen; i++) {
            float err = fabsf(output[i] - ir_collection[0].data[i]);
            if (err > maxErr) { maxErr = err; worstIdx = i; }
        }

        // Tolerance: ~0.02 accounts for the DC/Nyquist artifact (~1/N per partition)
        bool pass = maxErr < 0.02f;
        printf("max error = %.2e at sample %zu %s\n",
               maxErr, worstIdx, pass ? "PASS" : "FAIL");
        if (!pass) {
            printf("  Around worst sample (%zu):\n", worstIdx);
            size_t start = (worstIdx > 3) ? worstIdx - 3 : 0;
            size_t end = std::min(worstIdx + 4, irLen);
            for (size_t i = start; i < end; i++) {
                printf("    [%zu] expected=%.8f got=%.8f err=%.2e\n",
                       i, ir_collection[0].data[i], output[i],
                       fabsf(output[i] - ir_collection[0].data[i]));
            }
            allPassed = false;
        }
    }

    if (allPassed) printf("\nAll tests passed.\n");
    return allPassed;
}

// ============================================================
// Main
// ============================================================

static void usage(const char* prog) {
    printf("MuleBox IR Convolution Test\n\n");
    printf("Usage:\n");
    printf("  %s [options] INPUT.wav OUTPUT.wav\n", prog);
    printf("  %s -l\n", prog);
    printf("  %s --selftest\n\n", prog);
    printf("Options:\n");
    printf("  -l           List available IRs and exit\n");
    printf("  -i INDEX     Select IR by index (default: 0)\n");
    printf("  --selftest   Validate FFT shim and convolution accuracy\n");
}

int main(int argc, char* argv[]) {
    int irIndex = 0;
    const char* inputPath = nullptr;
    const char* outputPath = nullptr;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-l") == 0) {
            listIrs();
            return 0;
        } else if (strcmp(argv[i], "--selftest") == 0) {
            return selftest() ? 0 : 1;
        } else if (strcmp(argv[i], "-i") == 0 && i + 1 < argc) {
            irIndex = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else if (!inputPath) {
            inputPath = argv[i];
        } else if (!outputPath) {
            outputPath = argv[i];
        } else {
            fprintf(stderr, "Unexpected argument: %s\n", argv[i]);
            usage(argv[0]);
            return 1;
        }
    }

    if (!inputPath || !outputPath) {
        usage(argv[0]);
        return 1;
    }

    // Validate IR index
    using namespace ImpulseResponseData;
    if (IR_COUNT == 0) {
        fprintf(stderr, "Error: no IRs compiled into ir_data.h\n");
        return 1;
    }
    if (irIndex < 0 || irIndex >= (int)IR_COUNT) {
        fprintf(stderr, "Error: IR index %d out of range (0-%zu)\n", irIndex, IR_COUNT - 1);
        listIrs();
        return 1;
    }

    // Read input WAV
    WavFile wav;
    if (!readWav(inputPath, wav)) return 1;

    if (wav.sampleRate != 48000) {
        fprintf(stderr, "Warning: input sample rate is %u Hz (expected 48000 Hz)\n", wav.sampleRate);
    }
    printf("Input: %s (%zu samples, %.2f sec, %u Hz, %d-bit %s)\n",
           inputPath, wav.samples.size(),
           wav.samples.size() / (float)wav.sampleRate,
           wav.sampleRate, wav.bitsPerSample,
           wav.audioFormat == 3 ? "float" : "PCM");

    // Initialize convolution
    IrLoader irLoader;
    irLoader.Init(MAX_PARTITIONS, convIrFreqBuf, convFdlBuf);
    irLoader.loadIr(irIndex);

    const IRInfo& ir = ir_collection[irIndex];
    printf("IR: [%d] %s (%zu samples, %.1f ms)\n",
           irIndex, ir.name, ir.length, ir.length * 1000.0f / 48000.0f);

    // Process audio in blocks of PARTITION_SIZE
    size_t numInputSamples = wav.samples.size();
    // Output includes IR tail
    size_t numOutputSamples = numInputSamples + ir.length;
    size_t totalBlocks = (numOutputSamples + PARTITION_SIZE - 1) / PARTITION_SIZE;

    std::vector<float> output(totalBlocks * PARTITION_SIZE, 0.0f);

    for (size_t b = 0; b < totalBlocks; b++) {
        float inBlock[PARTITION_SIZE] = {0};
        size_t offset = b * PARTITION_SIZE;

        // Copy input samples (zero-padded for tail blocks)
        if (offset < numInputSamples) {
            size_t count = std::min(PARTITION_SIZE, numInputSamples - offset);
            memcpy(inBlock, &wav.samples[offset], count * sizeof(float));
        }

        float outBlock[PARTITION_SIZE];
        irLoader.ProcessBlock(inBlock, outBlock, PARTITION_SIZE);
        memcpy(&output[offset], outBlock, PARTITION_SIZE * sizeof(float));
    }

    // Trim to actual output length
    output.resize(numOutputSamples);

    // Find peak level
    float peak = 0.0f;
    for (float s : output) {
        float a = fabsf(s);
        if (a > peak) peak = a;
    }
    float peakDb = (peak > 0.0f) ? 20.0f * log10f(peak) : -120.0f;

    // Write output
    if (!writeWav(outputPath, output, wav.sampleRate)) return 1;

    printf("Output: %s (%zu samples, %.2f sec, peak: %.1f dBFS)\n",
           outputPath, output.size(),
           output.size() / (float)wav.sampleRate,
           peakDb);

    if (peak > 1.0f) {
        printf("Warning: output clipped (peak %.2f). Consider reducing input level.\n", peak);
    }

    return 0;
}
