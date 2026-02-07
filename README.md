# BBS_BUFFER_A
### BAMBU Filament Buffer alternative with super cheap component
Minimal, fast, and hardware‑focused firmware for the CH32V003.  
This project currently ships a clean **ADC → PWM** control loop with **button‑driven calibration** stored in flash.

Built with GPT Codex 5.2.

Based on the original BBS Buffer A project: `https://github.com/4061N/BBS-Buffer-A`

## Highlights
- 10‑bit ADC read on PA2, mapped to PWM duty.
- PWM output on PC4 (TIM1 CH4) by default.
- Short/long press calibration on PC1:
  - Short press → save **min ADC**
  - Long press → save **max ADC**
- Calibration stored in on‑chip flash (EEPROM‑style).
- UART debug prints on PD5 (115200 8N1).

## Quick Start (PlatformIO)
From repo root:
```bash
pio run -e genericCH32V003A4M6
pio run -t upload -e genericCH32V003A4M6
```

## Pin Map (current firmware)
- **PA2**: ADC input
- **PC4**: PWM output (TIM1 CH4)
- **PC1**: Button input (internal pull‑up, active‑low)
- **PD5**: UART TX (115200 8N1)
- **PC2**: WS2812 data (optional, not used by default)

## Calibration Behavior
On **button release**:
- Press < `BUTTON_LONG_MS` → save min ADC
- Press ≥ `BUTTON_LONG_MS` → save max ADC

Saved values are written to the **last flash page** and loaded on boot.  
If flash data is invalid, firmware falls back to defaults.

## Key Config Macros (src/main.c)
- `USE_CALC` — enable/disable flash calibration
- `UART_ENABLE` — UART debug prints
- `PWM_USE_PC4` — choose PWM pin (PC4 vs PA1)
- `ADC_MIN_DEFAULT`, `ADC_MAX_DEFAULT` — fallback range
- `BUTTON_LONG_MS` — long‑press threshold

## Files of Interest
- `src/main.c` — main control loop
- `src/calibration.c` / `include/calibration.h` — flash storage helpers

## Notes
Flash writes erase a full page. Avoid saving excessively in tight loops.

## Recommended 3D Model
[BBS Buffer A XMCU Series Buffer 3D model](https://makerworld.com/en/models/2035894-bbs-buffer-a-xmcu-series-buffer#profileId-2196013)
