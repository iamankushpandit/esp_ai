# FNK0104B hardware reference notes (verified from sources)

Board: Freenove FNK0104B (identical design sold as LCDWIKI ES3C28P). The chip is an
ESP32-S3R8, meaning 8 MB octal PSRAM in-package, plus an external 16 MB 25VQ128 flash.
Gume measured it on the bench as rev v0.2, 16 MB quad flash and 8 MB OPI PSRAM
(`Gume/platformio.ini:111-115`).

Audited 2026-09-18. All paths are relative to `.refs/`. Abbreviations used below:
- **G** = Gume (owner's code, confirmed on hardware; highest trust).
- **F** = Freenove official repo.
- **SCH** = `freenove/Schematic/2.8inch_ESP32-S3_Display_Schematic.pdf` (1 page, LCDWIKI, dated 2025/6/11). Net names were read from the rendered page. The page does not give line numbers.
- Zipped Freenove libraries were extracted to scratch for reading. They are cited as `zip -> inner/path:line`.

Confidence tags:
- **[HW]** = G says it was measured on the real board.
- **[SRC]** = code or vendor setup only.
- **[SCH]** = read off the schematic.

---

## 1. GPIO table

| Function | GPIO | Notes | Sources |
|---|---|---|---|
| **LCD SPI MOSI** | 11 | | G `Gume/platformio.ini:203`; F `Libraries/FNK0104AB/TFT_eSPI_Setups_v1.3.zip -> TFT_eSPI_Setups/FNK0104AB_2.8_240x320_ILI9341.h:211` [HW] |
| **LCD SPI SCLK** | 12 | | G `platformio.ini:204`; F setup `:212` [HW] |
| **LCD SPI MISO** | 13 | Wired to panel SDO per SCH (`LCD_MISO`). Gume found the ILI9341 ID read never returned a real ID on any board (always `00` or `FF`) and that the read corrupted the panel state. Do not read the panel. | G `platformio.ini:202`, `Gume/CHANGELOG.md:1119-1136`; F setup `:210` |
| **LCD CS** | 10 | | G `platformio.ini:205`; F setup `:213` [HW] |
| **LCD DC (RS)** | 46 | Strapping pin. Sampled at reset, free afterwards. | G `platformio.ini:206`, `Gume/src/s3_diag.cpp:69-71`; F setup `:214` [HW] |
| **LCD RST** | none (-1) | Panel RESET is tied to the chip RESET/EN net (SCH), so use software reset `0x01`. | G `platformio.ini:207`; F setup `:216`; SCH |
| **LCD SPI host / clock** | TFT_eSPI `USE_HSPI_PORT` = **SPI3_HOST** on S3. Write clock 40 MHz, read clock 16 MHz (G) or 20 MHz commented out (F). SPI mode 0. | Pins 10/11/12/13 are the S3's **SPI2 (FSPI) IOMUX pins**. Native IDF can use SPI2_HOST for direct IOMUX routing; either host works through the GPIO matrix at 40 MHz. | G `platformio.ini:210-212`; F setup `:362`, `:375`; `TFT_eSPI_v2.5.43.zip -> TFT_eSPI/Processors/TFT_eSPI_ESP32_S3.h:27,54,69-71`; `TFT_eSPI.h:131-136` (mode 0) |
| **Backlight** | 45 | **Active HIGH.** Drives the gate of Q4 (BSS138 low-side N-FET, 10K gate pulldown R32, 5.6 ohm LED resistor R33), so it is **PWM-capable**. G uses LEDC at 5 kHz, 8-bit. 45 is a strapping pin (VDD_SPI voltage). The pulldown keeps it low (3.3 V flash), which is correct. | G `include/boards/fnk0104b.h:63-64`, `src/hal/BoardPower.cpp:62-71,278-289`, blink test `s3_diag.cpp:1060-1076` [HW]; F setup `:132-133`; SCH "High level lighting backlight" |
| **I2C SDA** (touch + codec, shared) | 16 | 4.7K pull-ups R29/R30 to 3V3 (SCH). Same net is `CTP_SDA`/`AU_SDA`/`IIC_SDA` and is also on the IIC header P4. | G `fnk0104b.h:86`, `s3_diag.cpp:72`; F `Sketch_11.1_Touch.ino:20`, `Sketch_07.2_Echo.ino:56` [HW] |
| **I2C SCL** (shared) | 15 | Same as SDA (`CTP_SCL`/`AU_SCL`/`IIC_SCL`). | G `fnk0104b.h:87`; F `Sketch_11.1_Touch.ino:21`, `Sketch_07.2_Echo.ino:55` [HW] |
| **I2C speed** | 400 kHz | Both sources. | G `fnk0104b.h:90`, `s3_diag.cpp:883`; F `Sketch_07.2_Echo.ino:57` |
| **Touch FT6336U addr** | 0x38 | SCH note: "IIC Slave device address: 0x38". | G `fnk0104b.h:89`; F `FT6336U_v1.0.2.zip -> .../src/FT6336U.h:16` [HW] |
| **Touch INT** | 17 | Active-low, open-drain. 10K pull-up R17 to 3V3 (SCH). G also enables the internal pull-up. **Neither G nor F uses the interrupt; both poll over I2C.** | G `fnk0104b.h:84-85`, `src/hal/Board.cpp:106`; F `Sketch_11.1_Touch.ino:23` [HW] |
| **Touch RST** | 18 | Active-low. 10K pull-up R18 to 3V3 (SCH). The chip must be released from reset before the first I2C transaction or it NAKs. | G `fnk0104b.h:88`, `Board.cpp:111-121,152-154`, `s3_diag.cpp:868-874`; F `Sketch_11.1_Touch.ino:22` [HW] |
| **ES8311 addr** | 0x18 | CE pin tied to GND through R16 0R (SCH), which gives 0x18. **Shares the I2C bus with touch.** | G `fnk0104b.h:68,144`; F `Sketch_07.2_Echo/es8311.h:20` [HW] |
| **I2S MCLK** | 4 | | G `fnk0104b.h:145`; F `Sketch_07.2_Echo.ino:49` [HW] |
| **I2S BCLK (SCLK)** | 5 | | G `fnk0104b.h:146`; F `Echo.ino:50` [HW] |
| **I2S WS / LRCK** | 7 | | G `fnk0104b.h:147`; F `Echo.ino:53` [HW] |
| **I2S DOUT** (ESP to codec DSDIN, speaker) | 8 | SCH net is named **`I2S_DI`**, which is codec-centric. | G `fnk0104b.h:148`; F `Echo.ino:52` [HW] |
| **I2S DIN** (codec ASDOUT to ESP, mic) | 6 | SCH net is named **`I2S_DO`**, which is codec-centric. Mic capture was verified by record-and-playback in G. | G `fnk0104b.h:149`, `s3_diag.cpp:106`; F `Echo.ino:51` ("I2S_DINT") [HW] |
| **Amp enable** (SC8002B SHUTDOWN) | 1 | **Active LOW**: LOW = amp on, HIGH = shutdown. SCH shows R26 10K from 3V3 to `AUDIO_EN`, so the **amp defaults OFF** at reset and in deep sleep. | G `fnk0104b.h:150-151`, `s3_diag.cpp:886-896`, `src/hal/BoardAudioBackend.cpp:117-122`; F `Echo.ino:54,63-64` (driven LOW once, never raised) [HW] |
| **Speaker** | n/a | SC8002B BTL output drives the SP+/SP- connector (JP3). There is no GPIO. | SCH; G `fnk0104b.h:136-137` |
| **Microphone** | n/a | **Analog** MEMS mic LMA2718B381-OA7, AC-coupled into ES8311 MIC1P/MIC1N (differential), powered from AU_VCC3V3 (a separate 3.3 V LDO U4). Not PDM. | SCH "MIC control circuit"; G `s3_diag.cpp:540`; F `es8311.cpp:306` (`digital_mic=false`) |
| **SDMMC CLK** | 38 | | F `Sketch_06.1_SDMMC_Test.ino:22`, `Echo.ino:41`; G `fnk0104b.h:105` (comment only) [SRC] |
| **SDMMC CMD** | 40 | | F `Sketch_06.1...ino:21`; G `fnk0104b.h:105` [SRC] |
| **SDMMC D0** | 39 | | F `...ino:23`; G `fnk0104b.h:105` [SRC] |
| **SDMMC D1** | 41 | | F `...ino:24`; G `fnk0104b.h:105` [SRC] |
| **SDMMC D2** | 48 | | F `...ino:25`; G `fnk0104b.h:105` [SRC] |
| **SDMMC D3** | 47 | | F `...ino:26`; G `fnk0104b.h:105` [SRC] |
| **SD bus width** | **4-bit** | `SD_MMC.begin("/sdcard", mode1bit=false, ...)`. Card-detect is not wired (SCH slot pin 9 is NC). | F `Sketch_06.1_SDMMC_Test/driver_sdmmc.cpp:5` |
| **Battery ADC** | 9 = **ADC1_CH8** | Divider R14 200K / R15 200K plus C16 100 nF, ratio **x2.0**. Uses 11/12 dB attenuation. | G `fnk0104b.h:155-169`, `BoardPower.cpp:103-118`, `s3_diag.cpp:77,845-847` [HW]; F `Sketch_05.1_Battery_Voltage.ino:14,24`; SCH |
| **BOOT button** | 0 | Active LOW, 10K external pull-up R10 (SCH KEY2). Strapping pin, so input only. | G `fnk0104b.h:98-101`, `Board.cpp:90-100`; F `Sketch_03.1_Button_RGB.ino:21` [HW] |
| **RESET button** | EN/CHIP_PU | KEY1, not a GPIO. | SCH |
| **RGB LED** | 42 | **One WS2812B** (addressable, GRB, powered from +5 V). Not PWM. Use the RMT/`led_strip` driver. G does not drive it. | F `Sketch_02.1_LedPixel.ino:16` (`TYPE_GRB`, count 1), `Sketch_03.1_Button_RGB.ino:17`; G `fnk0104b.h:119-131` |
| USB | 19 (D-) / 20 (D+) | Native USB-Serial/JTAG. There is **no USB-UART bridge**, so console output goes to USB-Serial/JTAG. | SCH; G `platformio.ini:379-381` |
| UART0 | 43 TX / 44 RX | Broken out on header P2 through 100 ohm resistors. | SCH |
| Free / expansion | 2, 3, 14, 21 (SCH nets IO2/IO3/IO14/IO21 on headers P3/P4) | Low confidence: the exact header pinout was read off the schematic image. | SCH |
| Not available | 26-32 (flash), 33-37 (octal PSRAM) | ESP32-S3R8. | SCH / chip |

### Disagreements, Gume vs Freenove

- **Pins: none.** Every pin G uses matches F exactly. G's SD and LED pins come from F (G's comments only). G verified display, touch, audio and battery on hardware.
- **Amp polarity.** F never states it. G established active-LOW on hardware (`s3_diag.cpp:886-894`), and the SCH confirms it (SC8002B SHUTDOWN is active-high, with a pull-up).
- **ADC volume reg 0x17 (mic capture).** F's `Sketch_07.1_Music/es8311.cpp:461` leaves `es8311_microphone_config()` commented out. Only `Sketch_07.2_Echo/es8311.cpp:460` calls it. G measured that without reg17 the capture peaks at 1 count in 32767 (`s3_diag.cpp:531-539`).
- **DAC volume curve.** F maps `vol*256/100-1` linearly onto reg 0x32 (`es8311.cpp:367`). G found this is wrong because the register is dB-linear, and replaced it (§3).
- **MCLK "must come from the APLL" (G `fnk0104b.h:139-141`, `BoardAudioBackend.cpp:290`, `s3_diag.cpp:564-568`) is not true for the ESP32-S3.** The S3 has no APLL. IDF v6.1 `components/soc/esp32s3/include/soc/soc_caps.h` has no `SOC_CLK_APLL_SUPPORTED`/`SOC_I2S_SUPPORTS_APLL` (the esp32 one does, at lines 221/362). S3 I2S clock sources are PLL_F240M, PLL_F160M, XTAL and external (`clk_tree_defs.h:320-333`). The legacy driver ignored `use_apll`, so G actually ran on PLL_F160M with the fractional divider, and that worked on hardware. In IDF v6.1, use `I2S_CLK_SRC_DEFAULT` (or `PLL_240M`) with `mclk_multiple = 384`. G's "audioUsedApll" log value is not evidence.

---

## 2. ILI9341 (TFT_eSPI `ILI9341_2_DRIVER`, used by both G and F)

Source: `TFT_eSPI_v2.5.43.zip -> TFT_eSPI/TFT_eSPI.cpp:680-790` and `TFT_Drivers/ILI9341_Init.h:127-247`.

**Sequence** (RST=-1, so software reset first):

```
0x01 SWRST                      ; delay 150 ms
0xCF 00 C1 30
0xED 64 03 12 81
0xE8 85 00 78
0xCB 39 2C 00 34 02
0xF7 20
0xEA 00 00
0xC0 10                         ; PWCTR1
0xC1 00                         ; PWCTR2
0xC5 30 30                      ; VMCTR1
0xC7 B7                         ; VMCTR2
0x3A 55                         ; COLMOD 16-bit RGB565
0x36 08                         ; MADCTL (BGR), overwritten by rotation below
0xB1 00 1A                      ; FRMCTR1
0xB6 08 82 27                   ; DFUNCTR
0xF2 00                         ; 3-gamma off
0x26 01                         ; gamma curve 1
0xE0 0F 2A 28 08 0E 08 54 A9 43 0A 0F 00 00 00 00
0xE1 00 15 17 07 11 06 2B 56 3C 05 10 0F 3F 3F 0F
0x2B 00 00 01 3F                ; PASET 0..319
0x2A 00 00 00 EF                ; CASET 0..239
0x11 SLPOUT                     ; delay 120 ms
0x29 DISPON
0x21 INVON                      ; because TFT_INVERSION_ON (TFT_eSPI.cpp:774-776)
0x36 <rotation>                 ; setRotation()
```

- **Colour order is BGR** (MADCTL bit3 = 1). **Inversion is ON (0x21).** G says both are "measured, not assumed … an unusual pair" (`platformio.ini:190-193`, `s3_diag.cpp:29-31`). F's setup has the same values (`:77`, `:116`).
- G's profile `invertColours=false` (`fnk0104b.h:65`) only controls an *extra* `invertDisplay()` call. INVON still comes from the `TFT_INVERSION_ON` build flag.
- **MADCTL per rotation** with BGR (`ILI9341_Rotation.h:8-43`):
  - rot0 = `0x48` (MX|BGR), portrait 240x320, **USB-C at the bottom** [HW]
  - rot1 = `0x28` (MV|BGR), 320x240, USB-C on the right [HW]
  - rot2 = `0x88` (MY|BGR)
  - rot3 = `0xE8` (MX|MY|MV|BGR), 320x240, USB-C on the left. **G's landscape choice** (`fnk0104b.h:50-61`).
- **Byte order:** TFT_eSPI sends RGB565 **MSB-first** on the wire. In native esp_lcd, pass big-endian pixels (byte-swap uint16 on a little-endian CPU), or rely on the panel driver's handling.
- **Quirks from G:**
  1. **Never read the panel** (RDDID / `readcommand8`). It left the panel mid-command, with a crushed top bar and the rest unpainted (`CHANGELOG.md:1119-1136`).
  2. A full-screen repaint at 40 MHz is about 150 KB and about 30 ms, and shows as a visible flash. G fixed flicker by repainting only regions that changed (`CHANGELOG.md` 5.3.0 "screen flash every couple of seconds", line 2127).
  3. GPIO45 and 46 are strapping pins (`s3_diag.cpp:69-71`).
  4. `TFT_INVERT_COLORS` is ignored by TFT_eSPI's ILI9341 init (`Board.cpp` comment before `invertDisplay`). This does not matter natively.
- IDF v6.1 core `esp_lcd` has no ILI9341 driver (only st7789/ssd1306). Use the managed component `espressif/esp_lcd_ili9341` or send the sequence above yourself.

---

## 3. ES8311

**Driver provenance.** F's `Sketch_07.x/es8311.cpp` is Espressif's standalone **`es8311` component** (header "SPDX 2015-2022 Espressif, Apache-2.0", `es8311.cpp:1-5`), copied and slightly edited. It is **not `esp_codec_dev`**. It uses the **legacy I2C API** (`i2c_master_write_to_device`, `es8311.cpp:145-156`) on `I2C_NUM_0`, riding on Arduino `Wire`. G does not use a library. It writes the same register values directly over `Wire`, "lifted from their es8311.c" (`s3_diag.cpp:94-100`).

**Verified init: G `s3_diag.cpp:500-549`, which records and plays back on hardware.** The app version is `BoardAudioBackend.cpp:241-277`; it is playback-only and skips the ADC regs.

| Reg | Value | Meaning |
|---|---|---|
| 00 | 1F, wait 20 ms, 00, 80 | reset, then power-on |
| 01 | 3F | all clocks on, MCLK from the MCLK pin (bit7=0), not inverted |
| 02 | `(rd & 07) \| (2<<5) \| (1<<3)` | pre_div=3, pre_multi code 1 (= x2) |
| 03 | 10 | fs_mode single, adc_osr 0x10 |
| 04 | 10 | dac_osr 0x10 |
| 05 | 00 | adc_div = dac_div = 1 |
| 06 | `(rd & E0) \| 03` | bclk_div 4 |
| 07 | `(rd & C0) \| 00` | lrck_h |
| 08 | FF | lrck_l, so LRCK = 256 x internal clk |
| 00 | `rd & BF` | slave mode |
| 09 / 0A | 0C / 0C | SDP in/out: I2S, 16-bit |
| 0D | 01 | analog power up |
| 0E | 02 | PGA + ADC modulator on |
| 12 | 00 | DAC power up |
| 13 | 10 | output (HP drive) enable |
| 1C | 6A | ADC EQ bypass, DC-offset cancel |
| 37 | 08 | DAC EQ bypass |
| **14** | **1A** | analog mic (MIC1P/N) selected, PGA gain max (bits[3:0]=0xA). Bit6=0 means not DMIC. |
| **17** | **C8** | **ADC volume. Resets to minimum and must be set** (0xBF = 0 dB, so C8 = +4.5 dB) |
| 16 | 07 (G default, `s3_diag.cpp:144,542`) | ADC digital scale-up 0..7 in 6 dB steps (F enum `ES8311_MIC_GAIN_0DB..42DB`, `es8311.h:36-46`). F leaves it at reset. G makes it tunable with the 'g' key. |
| 32 | computed | DAC volume, see below |

- **Clocking:** 16 kHz, MCLK = 384 x fs = **6.144 MHz**. This is the coefficient row `{6144000,16000,0x03,0x01,0x01,0x01,0x00,0x00,0xff,0x04,0x10,0x10}` in F `es8311.cpp:71`. In that table `pre_multi` is already the register code (0=1x, 1=2x), so G's comment "pre_multi 1" is correct. G's app plays at 16 kHz (`BoardAudioInternal.h:42-44`). The codec only depends on the MCLK/fs ratio.
- F is inconsistent here:
  - Echo `es8311.h:25-26` says 44100/256, but it runs I2S at 16 kHz. It still works because the ratio is 256 on both sides.
  - Music `es8311.h` says 16000/384 while I2S runs at 44.1 kHz.
- **I2S format:** Philips standard I2S, 16-bit, ESP is master, codec is slave.
  - G uses the legacy driver: stereo `RIGHT_LEFT`, `mclk_multiple=384`, `dma 6x256`, `tx_desc_auto_clear` (`s3_diag.cpp:551-586`, `BoardAudioBackend.cpp:279-310`).
  - F Echo uses Arduino `ESP_I2S`: `I2S_MODE_STD, 16000, 16-bit, I2S_SLOT_MODE_MONO, I2S_STD_SLOT_LEFT` (`Sketch_07.2_Echo.ino:68-69`), then `recordWAV`/`playWAV` (`:97,102`).
  - F Music uses 44100, stereo, slot LEFT (`Sketch_07.1_Music.ino:71`), then ESP32-audioI2S for MP3 (`:95-96`).
- **Mic slot:** the ES8311 ADC is mono. F captures **LEFT slot mono**. G takes frame index 0 of the legacy stereo buffer ("left channel only", `s3_diag.cpp:718`) and got an audible capture. In IDF v6 `i2s_std` RX, use `I2S_SLOT_MODE_MONO` with `slot_mask = I2S_STD_SLOT_LEFT`.
- **DAC volume (reg 0x32):** dB-linear, 0.5 dB/step, `0xBF` = 0 dB, `0x00` = mute. G formula: `reg = round(0xBF + 2*20*log10(pct/100))`, clamped to 1..0xBF (`BoardAudioBackend.cpp:153-167`, `s3_diag.cpp:471-479`). Product ceiling is 85% (about -1.4 dB), set by listening (`fnk0104b.h:152`, `src/hal/Board.h:183-208`). F's linear formula gives 0x98 = -19.5 dB at "60%" and +32 dB at 100% (`CHANGELOG.md:2190-2200`).
- **Pop/click handling (G):**
  - Amp is OFF at boot (driven HIGH), switched on only while audio plays, and kept on for a **2 s tail** after the last sample, because toggling the amp itself pops (`BoardAudioBackend.cpp:100-122,177-183,228-237`).
  - 4 ms fade in and out on tones (`s3_diag.cpp:588-618`).
  - `tx_desc_auto_clear=true`, so an idle DMA sends zeros.
  - **The amp is muted while recording** to stop feedback, and the **first 4 RX buffers are discarded** after the amp state changes (`s3_diag.cpp:695-708,733`).
  - F does no pop handling. It holds the amp permanently on (LOW).

---

## 4. FT6336U

- Reads in G (`BoardTouch.cpp:22-26,149-174`; probe `s3_diag.cpp:79-82,783-843`):
  - One burst read of **5 bytes from 0x02**: TD_STATUS, then P1 XH, XL, YH, YL.
  - `n = b0 & 0x0F`. `x = (b1&0x0F)<<8 | b2`, `y = (b3&0x0F)<<8 | b4`.
  - The probe also reads chip ID `0xA3` and firmware `0xA6`.
- F library (`FT6336U_v1.0.2.zip -> FT6336U_CTP_Controller/src/FT6336U.cpp:25-48,218-247,249-260`) reads one byte per register, with **`delay(10)` inside every `readByte`**, which is slow. It supports 2 touch points, event bits `>>6`, and ID `>>4`. Its `begin()` pulses RST low for 10 ms, then waits 500 ms. G waits 20 ms low and 300 ms after release in the probe. In the app, G overlaps the settle time with panel init.
- **INT:** configured as input (with pull-up in G) but **never used** by either source; both poll every frame. Nobody writes `G_MODE` (0xA4, polling vs trigger INT mode) or power mode 0xA5. The INT behavior is therefore **unverified on this board**. Test it before relying on it for wake.
- **Coordinates:** reported in the native portrait frame (240x320), where rot0 = USB at the bottom.
  - G probe mapping, verified at all 4 rotations (`s3_diag.cpp:204-216`):
    - rot0: `(x,y)`
    - rot1: `(y, 239-x)`
    - rot2: `(239-x, 319-y)`
    - rot3: `(319-y, x)`
  - F agrees for rot1: `x = raw.y; y = 240 - raw.x` (`Sketch_12.1_TFT_Touch_Draw.ino:156-160`, off by one).
  - G's app composes this through `mapTouch` + `pollTouch` (`BoardTouch.cpp:356-365,386-403`).
- Chip is present at 0x38. Coordinates arrive as pixels, so no calibration is needed (`BoardTouch.cpp:78-89`).

---

## 5. SDMMC

- Pins: CLK 38, CMD 40, D0 39, D1 41, D2 48, D3 47. **4-bit**, all through the GPIO matrix. The S3 SDMMC has no fixed IOMUX pins, so any GPIO works.
- Frequency: F passes `BOARD_MAX_SDMMC_FREQ` (`driver_sdmmc.cpp:5`). Its value comes from the arduino-esp32 core and is not in this repo. **Not verified here.** Start at 20 MHz (`SDMMC_FREQ_DEFAULT`) and try 40 MHz.
- Pull-ups: SCH shows **four 10K pull-ups (R34-R37) to 3V3** on the card bus. Which net each resistor sits on is not legible in the extracted text; it appears to be the data/CMD lines. Enable internal pull-ups too (`SDMMC_SLOT_FLAG_INTERNAL_PULLUP`) as a belt-and-braces measure. No card-detect or write-protect lines.
- **G has never run the SD slot** (`fnk0104b.h:103-117`, README "SD card" row). Only F's sketch exercises it. It is unverified on the owner's board.

---

## 6. Gume build system and hardware quirks

- **PlatformIO + Arduino**:
  - `platform = platformio/espressif32@7.0.1`, board `esp32-s3-devkitc-1`, `framework = arduino` (`platformio.ini:116-128`).
  - This is Arduino core **2.0.17 = IDF 4.4** (`s3_diag.cpp:95-96`), so it uses the **legacy** `driver/i2s.h`.
  - Flash settings: `flash_mode=qio`, `memory_type=qio_opi` ("get it wrong and the board … fails at the first PSRAM access"), 16 MB, `huge_app.csv`.
  - `ARDUINO_USB_CDC_ON_BOOT=1` is required because there is no UART bridge (`platformio.ini:379-381,392-393,506-507`).
  - Libraries: TFT_eSPI 2.5.43 configured by `-D` flags.
  - The owner's product env is `app_fnk0104b`; the bring-up probe is `s3diag`.
- Quirks and workarounds documented by G:
  1. Amp enable is active LOW (§1).
  2. ADC volume reg17 resets to min, so the mic records silence (§3).
  3. DAC volume register is in dB (§3).
  4. The APLL claim is wrong for the S3 (§1).
  5. Do not read the panel ID (§2).
  6. Release touch RST before the first I2C access (`Board.cpp:104-121,148-155`).
  7. Merged-image **bootloader offset on S3 is 0x0, not 0x1000** (`CHANGELOG.md:1140-1154`).
  8. A merged image write erases NVS (`CHANGELOG.md:1154-1160`).
  9. "One 2.8-inch board's USB drops off the bus as its app starts", so the boot banner is lost. Open the serial port with DTR/RTS low to avoid a reset (`.claude/skills/braino-boards/SKILL.md:97-107`).
  10. Battery: with a pack fitted on USB it reads 3.93 V, while USB with no pack reads 4.09-4.16 V. **A no-pack board reads higher, so battery presence cannot be detected**, and charge inference is unvalidated (`fnk0104b.h:155-164`). The TP4054 CHRG pin is not routed to a GPIO (SCH).
  11. I2C is shared by touch and codec. Avoid long transactions from one owner while the other polls (`fnk0104b.h:68-71`).

---

## 7. Reusability in native ESP-IDF v6.1 and licences

**Nothing is reusable as-is.** Everything in G is Arduino (`Wire`, `digitalWrite`, `ledc*`, `analogRead`, legacy `driver/i2s.h`, TFT_eSPI). IDF v6.1 has **no legacy I2S driver** (only `esp_driver_i2s/include/driver/i2s_std.h` etc.). The legacy `driver/i2c.h` still exists under `components/driver/i2c` but is deprecated and cannot share a port with `i2c_master`.

What is worth porting (logic and constants, not code):

| Path | What | Deps to replace |
|---|---|---|
| `Gume/src/s3_diag.cpp:432-549` | ES8311 register init for playback **and mic** | Wire → `i2c_master_transmit` / `transmit_receive` |
| `Gume/src/s3_diag.cpp:551-586,686-756` | full-duplex I2S setup, record/playback, amp gating | legacy i2s → `i2s_new_channel` + `i2s_channel_init_std_mode` (TX+RX on one controller) |
| `Gume/src/hal/BoardAudioBackend.cpp:117-183` | dB volume map, amp tail logic | `gpio_set_level`, `esp_timer` |
| `Gume/src/hal/BoardTouch.cpp:149-174,356-403` | FT6336U burst read + rotation map | Wire |
| `Gume/src/hal/Board.cpp:104-155` | touch reset / I2C bring-up ordering | gpio / i2c_master |
| `Gume/src/hal/BoardPower.cpp:62-118` | backlight LEDC 5 kHz/8-bit, battery ADC x2 | `ledc`, `adc_oneshot` + `adc_cali` (curve fitting on S3) |
| `Gume/include/boards/fnk0104b.h` | pin constants | none (plain constexpr) |

Alternatives for the codec:
- F's `es8311.cpp`, **Apache-2.0**, reusable. Port its two I2C helpers to `i2c_master`, or use the Espressif component registry `espressif/es8311` / `esp_codec_dev` directly; check the current API.
- Apply G's fixes on top: call `es8311_microphone_config()`, and use the dB-correct volume.

Licences:
- **Gume: GPL-3.0-or-later**, copyright iamankushpandit (`Gume/LICENSE`, `Gume/NOTICE.md`, SPDX headers in every file). "Braino!" is a trademark. As copyright holder, the owner can relicense their own code. Anyone else copying it takes on the GPL.
- **Freenove repo: CC BY-NC-SA 3.0** (`freenove/LICENSE.txt:1`). **Non-commercial**, and a poor fit for code. Some embedded files carry their own licences:
  - `es8311.cpp`/`.h`: Apache-2.0 (Espressif)
  - FT6336U library: MIT (Atsushi Sasaki, `FT6336U_v1.0.2.zip -> .../LICENSE`)
  - TFT_eSPI: its own mixed FreeBSD/MIT licence
- Treat F's sketches and `driver_sdmmc.cpp` as CC BY-NC-SA. Take facts (pin numbers) only, not code.

---

## 8. Touch INT and wake from sleep

- **Touch INT = GPIO17, which is RTC-capable.** ESP32-S3 RTC GPIOs are 0-21, and IDF v6.1 `soc/esp32s3/include/soc/soc_caps.h:282` has `SOC_RTCIO_PIN_COUNT 22`. EXT0 and EXT1 wake are supported (`soc_caps.h:382-383`).
- Wake options:
  - **Light sleep:** `gpio_wakeup_enable(17, GPIO_INTR_LOW_LEVEL)` works (any GPIO works).
  - **Deep sleep:** `esp_sleep_enable_ext0_wakeup(17, 0)`, or ext1 `ANY_LOW` with mask BIT(17).
- The INT line has an external 10K pull-up (R17), so no RTC pull-up is needed.
- Caveats, none verified on hardware:
  1. FT6336U G_MODE (0xA4) and power mode (0xA5) are never configured by G or F. Confirm that INT asserts on touch in the chip's monitor/idle mode, and do not put it into hibernate (hibernate needs a RST pulse to wake).
  2. Hold GPIO18 (touch RST) HIGH across deep sleep with `gpio_hold_en` / `gpio_deep_sleep_hold_en`. The R18 10K pull-up to 3V3 already keeps it deasserted.
  3. The amp pin GPIO1 has a pull-up, so the amp is off in deep sleep with no action needed.
  4. Backlight GPIO45 has a 10K gate pulldown, so the backlight is off in deep sleep.
  5. BOOT (GPIO0) is also RTC-capable and is an alternative wake key.
