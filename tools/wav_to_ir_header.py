#!/usr/bin/env python3
"""
WAV to IR Header Converter for MuleBox

Converts WAV files (impulse responses) to:
1. A raw binary file (ir_data.bin) containing concatenated float samples.
2. A C++ header (ir_data.h) with pointers to the data in QSPI flash.

Usage:
    python3 wav_to_ir_header.py [wav_files...] -o output.h --bin output.bin

Example:
    python3 tools/wav_to_ir_header.py irs/*.wav -o src/ir_data.h --bin build/ir_data.bin
"""

import argparse
import cmath
import math
import wave
import struct
import os
import sys
from pathlib import Path

# Constants
SAMPLE_RATE = 48000
MAX_IR_LENGTH_MS = 170  # Maximum IR length in milliseconds
MAX_IR_SAMPLES = int((MAX_IR_LENGTH_MS / 1000.0) * SAMPLE_RATE)  # 8,160 samples
MAX_IR_COUNT = 12

# QSPI Flash Layout
# The Daisy Seed IS25LP064A is 8MB (0x800000 bytes).
# Memory mapped from 0x90000000.
# The Bootloader takes first 128KB (or 256KB?). Actually bootloader is internal flash,
# but it uses QSPI for unexpected reasons? NO.
# The `BOOT_SRAM` configuration flashes the application to QSPI address 0x90040000.
# The application size is typically < 256KB.
# App (plus padding) ends at 0x90040000 + 512KB = 0x900C0000.

QSPI_BASE_ADDR = 0x90000000
# App is at 0x90040000
IR_DATA_START_OFFSET = 0x000C0000 # 768KB offset from QSPI base

def read_wav_file(filepath):
    """Read a WAV file and return normalized float samples."""
    with wave.open(str(filepath), 'rb') as wav:
        channels = wav.getnchannels()
        sample_width = wav.getsampwidth()
        framerate = wav.getframerate()
        n_frames = wav.getnframes()
        
        print(f"Processing: {filepath}")
        print(f"  Format: {channels}ch, {sample_width*8}-bit, {framerate}Hz, {n_frames} samples ({n_frames/framerate*1000:.0f}ms)")
        
        raw_data = wav.readframes(n_frames)
        
        if sample_width == 2:  # 16-bit
            samples = struct.unpack(f'<{n_frames * channels}h', raw_data)
            max_val = 32768.0
        elif sample_width == 3:  # 24-bit
            samples = []
            for i in range(0, len(raw_data), 3):
                b1, b2, b3 = raw_data[i:i+3]
                val = b1 | (b2 << 8) | (b3 << 16)
                if val & 0x800000:
                    val |= 0xFF000000
                val = struct.unpack('<i', struct.pack('<I', val & 0xFFFFFFFF))[0]
                samples.append(val)
            max_val = 8388608.0
        elif sample_width == 4:  # 32-bit
            samples = struct.unpack(f'<{n_frames * channels}i', raw_data)
            max_val = 2147483648.0
        else:
            raise ValueError(f"Unsupported bit depth: {sample_width * 8}")
            
        if channels == 2:
            samples = samples[::2]
        elif channels > 2:
            raise ValueError(f"Only mono and stereo WAV files supported")
            
        normalized = [s / max_val for s in samples]
        return framerate, normalized

def trim_or_pad_ir(samples, target_length=MAX_IR_SAMPLES):
    """Trim or pad IR to target length."""
    current_length = len(samples)
    if current_length > target_length:
        print(f"  Trimming from {current_length} to {target_length} samples")
        return samples[:target_length]
    elif current_length < target_length:
        print(f"  Padding from {current_length} to {target_length} samples")
        return samples + [0.0] * (target_length - current_length)
    else:
        return samples

def _fft_recursive(x):
    """Radix-2 Cooley-Tukey FFT."""
    N = len(x)
    if N <= 1: return x
    even = _fft_recursive(x[0::2])
    odd = _fft_recursive(x[1::2])
    T = [cmath.exp(-2j * cmath.pi * k / N) * odd[k] for k in range(N // 2)]
    return [even[k] + T[k] for k in range(N // 2)] + [even[k] - T[k] for k in range(N // 2)]

def _rfft(samples):
    """Real FFT via Cooley-Tukey."""
    N = len(samples)
    fft_len = 1
    while fft_len < N: fft_len <<= 1
    padded = list(samples) + [0.0] * (fft_len - N)
    full = _fft_recursive(padded)
    return full[:fft_len // 2 + 1]

def normalize_ir(samples):
    """Normalize IR so its peak frequency response magnitude is 0 dB."""
    spectrum = _rfft(samples)
    peak_mag = max(abs(c) for c in spectrum)
    if peak_mag > 0:
        gain = 1.0 / peak_mag
        peak_db = 20.0 * math.log10(peak_mag)
        print(f"  Freq-response normalization: peak was {peak_db:+.1f} dB, scaled by {gain:.4f}")
        return [s * gain for s in samples]
    return samples

def sanitize_name(filepath):
    """Convert filepath to valid C++ identifier."""
    name = Path(filepath).stem
    name = ''.join(c if c.isalnum() else '_' for c in name)
    if name[0].isdigit(): name = 'ir_' + name
    return name.lower()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('wav_files', nargs='+', help='Input WAV files')
    parser.add_argument('-o', '--output', required=True, help='Output C++ header path')
    parser.add_argument('--bin', required=True, help='Output binary data path')
    args = parser.parse_args()

    files = sorted(args.wav_files)[:MAX_IR_COUNT]
    ir_metas = [] 
    all_floats = [] # The raw float data buffer

    print(f"\nIR order (mapped to rotary positions 1-{len(files)}):")
    for i, f in enumerate(files):
        print(f"  Position {i+1}: {os.path.basename(f)}")

    current_offset_bytes = 0
    for filepath in files:
        try:
            print("")
            rate, samples = read_wav_file(filepath)
            
            if rate != SAMPLE_RATE:
                print(f"  WARNING: Sample rate is {rate}Hz (expected {SAMPLE_RATE}Hz)")
            
            samples = trim_or_pad_ir(samples)
            samples = normalize_ir(samples)
            
            name = sanitize_name(filepath)
            length = len(samples)
            
            # Append to master buffer
            all_floats.extend(samples)
            
            ir_metas.append({
                'name': name,
                'offset': current_offset_bytes,
                'length': length
            })
            
            current_offset_bytes += length * 4 # 4 bytes per float
            
        except Exception as e:
            print(f"Error processing {filepath}: {e}")
            sys.exit(1)

    # 1. Write Binary Blob
    print(f"\nWriting binary data to {args.bin}...")
    with open(args.bin, 'wb') as f:
        # Pack as little-endian floats
        blob = struct.pack(f'<{len(all_floats)}f', *all_floats)
        f.write(blob)
    print(f"  Size: {len(blob)} bytes")

    # 2. Write C++ Header
    print(f"Writing header to {args.output}...")
    
    header_lines = [
        "// Auto-generated IR data header",
        "// Do not edit manually - regenerate using wav_to_ir_header.py",
        "//",
        "// IR data is stored in QSPI flash separately from the firmware.",
        "// Accessed via memory-mapped pointers.",
        "",
        "#ifndef IR_DATA_H",
        "#define IR_DATA_H",
        "",
        "#include <cstddef>",
        "#include <cstdint>",
        "",
        "namespace ImpulseResponseData {",
        "",
        "// IR metadata",
        "struct IRInfo {",
        "    const char* name;",
        "    const float* data;  // Pointer to IR sample data in QSPI",
        "    size_t length;      // Sample count",
        "};",
        "",
        f"// Base address for IR data in QSPI",
        f"// APP is at 0x90040000. We place data at +512KB offset (0x00080000)",
        f"static constexpr uint32_t QSPI_IR_START_ADDR = {hex(QSPI_BASE_ADDR + IR_DATA_START_OFFSET)};",
        ""
    ]
    
    ir_names = []
    
    for meta in ir_metas:
        name = meta['name']
        offset = meta['offset']
        # Generate pointer definition
        # We use a macro or just a const pointer initialized with cast
        # Note: In C++, we can't initialize a pointer with a reinterpret_cast in a constexpr context easily
        # but these are file-scope consts.
        header_lines.append(f"// {name}")
        header_lines.append(f"static const float* {name} = (const float*)(QSPI_IR_START_ADDR + {offset});")
        ir_names.append(name)
        
    header_lines.append("")
    header_lines.append(f"constexpr size_t IR_COUNT = {len(ir_names)};")
    header_lines.append("")
    header_lines.append("static const IRInfo ir_collection[IR_COUNT] = {")
    for meta in ir_metas:
        name = meta['name']
        length = meta['length']
        header_lines.append(f'    {{"{name}", {name}, {length}}},')
    header_lines.append("};")
    
    header_lines.extend([
        "",
        "}  // namespace ImpulseResponseData",
        "",
        "#endif  // IR_DATA_H"
    ])
    
    with open(args.output, 'w') as f:
        f.write('\n'.join(header_lines))
        
    print("Done!")

if __name__ == '__main__':
    main()
