# MuleBox Impulse Responses

This directory contains the guitar cabinet impulse responses (IRs) shipped with MuleBox.

## Source and Attribution

All IRs in this directory are from **Djammincabs**, a free guitar cabinet IR collection by **Fred Kissell**.

> "They're not proprietary files, and free is good. You are welcome to use this free download in your projects. Please share them and spread them around."
>
> — Fred Kissell, [zystrix.com/djammincabs.htm](https://zystrix.com/djammincabs.htm)

The collection was captured from real guitar speaker cabinets using microphones.

**Credit:** Guitar Cabinet Impulse Responses by Fred Kissell / Djammincabs.
**Source:** https://zystrix.com/djammincabs.htm

---

## Included IRs

Files are sorted alphabetically, which maps directly to rotary switch positions 1–4 on the MuleBox hardware.

| Rotary position | Filename | Character | Source filename |
|-----------------|----------|-----------|-----------------|
| 1 | `01_dark_closed.wav` | Dark, full-bodied, prominent low-mids. Suits high-gain and classic rock tones. | Djammincabs Guitar IR -026.wav |
| 2 | `02_warm_mid.wav` | Warm, midrange-forward. Good for crunch and blues tones. | Djammincabs Guitar IR -086.wav |
| 3 | `03_balanced.wav` | Balanced across the frequency range. Versatile all-purpose character. | Djammincabs Guitar IR -041.wav |
| 4 | `04_bright_open.wav` | Bright, present, with strong upper-mids and air. Suits clean and chimey tones. | Djammincabs Guitar IR -076.wav |

The tonal character descriptions are based on frequency analysis (high-to-low energy ratio) of the IR waveforms.

**Format:** All files are stereo 24-bit 48kHz WAV, 5516 samples (≈115ms). The firmware build tool extracts the left channel.

---

## Adding or Changing IRs

The `irs/` directory can hold up to 12 WAV files (matching the 12 rotary switch positions). Files are sorted alphabetically and mapped to positions 1–12 in order.

After adding, removing, or replacing any WAV file, regenerate the firmware header:

```bash
python3 tools/wav_to_ir_header.py irs/*.wav -o src/ir_data.h
```

Requirements:
- **Filenames must not contain spaces** (use underscores or hyphens instead)
- WAV files must be **48kHz** (the tool warns but does **not** resample)
- Mono or stereo are both accepted (stereo: left channel is used)
- Maximum length: 170ms (8160 samples at 48kHz); longer files are trimmed
- Maximum 12 files; extras are ignored with a warning

Commit both the new WAV file(s) and the regenerated `src/ir_data.h`.

---

## Downloading More Djammincabs IRs

The full Djammincabs collection (100 guitar + 50 bonus guitar IRs) is freely available at:
https://zystrix.com/djammincabs.htm

All files in the collection are 48kHz 24-bit stereo WAV and ready to use with MuleBox without resampling.
