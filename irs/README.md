# MuleBox Impulse Responses

This directory contains the guitar cabinet impulse responses (IRs) shipped with MuleBox.

---

## Included IRs

Files are sorted alphabetically, which maps directly to rotary switch positions 1–6 on the MuleBox hardware.

| Rotary position | Filename | Character |
|-----------------|----------|-----------|
| 1 | `01_Black_1x12.wav` | Classic American 1x12 combo, scooped mids, glassy highs. |
| 2 | `02_Green_4x12.wav` | British 4x12 with Greenbacks, warm, woody, early breakup. |
| 3 | `03_V30_4x12.wav` | Modern 4x12 with Vintage 30s, punchy midrange and tight bass. |
| 4 | `04_Chime_2x12.wav` | British Class A 2x12, jangly, bright and upper-mid forward. |
| 5 | `05_Tweed_1x12.wav` | Vintage American 1x12, raw midrange, loose low end. |
| 6 | `06_Boutique_2x12.wav` | High-end 2x12, complex overtones, smooth balanced response. |

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
- Maximum length: 85.3ms (4096 samples at 48kHz); longer files are automatically truncated
- Maximum 12 files; extras are ignored with a warning

Commit both the new WAV file(s) and the regenerated `src/ir_data.h`.

---
