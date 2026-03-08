# MuleBox Desktop IR Test

Offline tool for testing the MuleBox IR convolution engine on macOS without hardware. Processes a WAV file through the production `ConvolutionEngine`, `IrLoader`, and `ir_data.h` code, producing a WAV output for listening comparison.

## Building

From the project root:

```bash
make desktop-test
```

Or from this directory:

```bash
make
```

Requires macOS with Apple Accelerate framework (included with Xcode / Command Line Tools).

## Usage

```
./build/mulebox_ir_test [options] INPUT.wav OUTPUT.wav

Options:
  -l           List available IRs and exit
  -i INDEX     Select IR by index (default: 0)
  --selftest   Validate FFT shim and convolution accuracy
  -h, --help   Show usage
```

### Examples

```bash
# List compiled IRs
./build/mulebox_ir_test -l

# Process a dry guitar recording through IR 0
./build/mulebox_ir_test -i 0 dry_guitar.wav wet_ir0.wav

# Compare all IRs
for i in 0 1 2 3; do
    ./build/mulebox_ir_test -i $i dry.wav wet_ir${i}.wav
done

# Run self-test to validate the FFT shim
./build/mulebox_ir_test --selftest
```

### Input requirements

- WAV format (PCM 16-bit, 24-bit, or 32-bit float)
- 48 kHz sample rate (warns if different)
- Mono or stereo (left channel extracted from stereo)

### Output

- 16-bit PCM WAV, mono, same sample rate as input
- Length = input + IR length (convolution tail is preserved)
- Reports peak level and warns if clipping occurs

## How it works

The production convolution engine (`convolution_engine.h`) depends on ARM CMSIS-DSP for FFT operations. To run on macOS, `arm_math.h` provides a drop-in shim that replaces the four CMSIS functions with Apple Accelerate (vDSP) equivalents:

| CMSIS function | Shim implementation |
|---|---|
| `arm_rfft_fast_init_f32` | `vDSP_create_fftsetup` + twiddle factor generation |
| `arm_rfft_fast_f32` | `vDSP_fft_zip` + CMSIS `stage_rfft`/`merge_rfft` (ported from C source) |
| `arm_cmplx_mult_cmplx_f32` | Scalar complex multiply loop |
| `arm_add_f32` | `vDSP_vadd` |

The `stage_rfft` and `merge_rfft` recombination steps are ported directly from the CMSIS source to ensure identical numerical behavior. Only the complex FFT backend differs (vDSP instead of ARM radix-8).

### Production code reused unchanged

- `src/convolution_engine.h` -- partitioned overlap-save convolution
- `src/ir_loader.h` -- IR selection and loading
- `src/ir_data.h` -- compiled IR coefficient data

### Known limitation

The CMSIS packed RFFT format stores DC and Nyquist frequency bins as a single complex pair `[DC, Nyquist]`. When `arm_cmplx_mult_cmplx_f32` multiplies two spectra, it treats this pair as a complex number, cross-contaminating DC and Nyquist. This introduces a per-sample error of ~1/N (~0.004 for N=256, about -48 dB). The same artifact occurs on real CMSIS hardware. It is inaudible for audio signals.
