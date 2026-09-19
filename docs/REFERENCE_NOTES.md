# Reference notes — index and decisions

Detailed, cited audits live in `docs/refnotes/`:

| File | Covers |
|---|---|
| [refnotes/hardware.md](refnotes/hardware.md) | FNK0104B pins, ILI9341/ES8311/FT6336U/SDMMC init and quirks (Gume Braino + Freenove) |
| [refnotes/stt.md](refnotes/stt.md) | lspr98/conformer-stt-s3 model, format, memory, SD-streaming analysis |
| [refnotes/llm.md](refnotes/llm.md) | therezor/cardputer-ai GPT-Neo Q4 engine; manjunathshiva/esp32-tinyllm memory architecture |
| [refnotes/wake_tts.md](refnotes/wake_tts.md) | esp-sr WakeNet/AFE, "Hey Story" options, PicoTTS and alternatives |

## Verified pin map (FNK0104B)

| Function | GPIO |
|---|---|
| LCD SPI MOSI/SCLK/CS/DC | 11 / 12 / 10 / 46 (RST = board reset, MISO 13 unused — never read the panel) |
| Backlight | 45, active HIGH (PWM ok) |
| I2C SDA/SCL (touch 0x38 + ES8311 0x18 share) | 16 / 15 @ 400 kHz |
| Touch INT / RST | 17 / 18 (both active low) |
| I2S MCLK/BCLK/LRCK | 4 / 5 / 7 |
| I2S DOUT (to codec) / DIN (from codec) | 8 / 6 |
| Amplifier enable | 1, **active LOW** |
| SDMMC CLK/CMD/D0/D1/D2/D3 | 38 / 40 / 39 / 41 / 48 / 47 |
| Battery ADC | 9 (ADC1 ch8), ×2 divider |
| BOOT button | 0 |

## Licensing summary

| Component | License | Use |
|---|---|---|
| conformer-stt-s3 code | Apache-2.0 | ported |
| conformer weights | CC-BY-4.0 | attribute NVIDIA + lspr98 |
| cardputer-ai code | MIT | ported |
| TinyTalk weights | non-commercial by dataset terms | demo only; flagged |
| esp-picotts | Apache-2.0 | ported |
| esp-sr | Espressif license, ESP chips only | later (wake bring-up) |
| microWakeWord | Apache-2.0 (training); ESPHome glue is GPL — **not used** | later |
| Gume | GPL-3.0 (owner's) | facts only |
| Freenove sketches | CC BY-NC-SA 3.0 | pin facts only |
| X11 misc-fixed font | public domain | linked |

## Key "gotchas" we must honor

1. ES8311 reg 0x17 (ADC volume) resets to minimum → mic records silence unless set (0xC8).
2. ES8311 reg 0x32 is dB-linear (0xBF = 0 dB).
3. ESP32-S3 has no APLL; use default I2S clock source with mclk_multiple 384.
4. Amp enable is active LOW; keep off except while playing; toggling pops.
5. Never read back from the ILI9341.
6. Conformer needs a contiguous 256 KB **internal** buffer; reserve at boot.
7. Conformer activation shifts are hard-coded to its exact model.bin.
8. esp-sr `esp_srmodel_deinit()` crashes in SD mode.
9. PicoTTS defaults to en-GB; the example disables the CPU1 watchdog because synthesis never yields.
10. GPT-2 tokenizer in cardputer-ai skips regex pre-split (empirically OK).
