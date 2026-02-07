# Repository Guidelines

## Project Structure & Module Organization
- `src/`: firmware sources in C, including `main.c`, startup code, and the linker script `ch32v003.ld`.
- `include/`: shared headers for the firmware (`.h` files).
- `lib/`: project-specific libraries, organized per PlatformIO conventions.
- `test/`: PlatformIO Test Runner tests (see `test/README`).
- `ref/`: reference/ported code used for guidance; not built by default.
- `.pio/`: generated build artifacts (do not edit by hand).
- `platformio.ini`: build environments and upload/debug configuration.

## Build, Test, and Development Commands
Use the PlatformIO CLI (`pio`) from the repo root:
- `pio run`: build the default environment.
- `pio run -e ch32v003f4p6_evt_r0`: build for the EVT board.
- `pio run -e genericCH32V003A4M6`: build for the generic board.
- `pio run -t upload`: flash via `minichlink` (as configured in `platformio.ini`).
- `pio device monitor -b 115200`: serial monitor at the configured baud.
- `pio test`: run unit tests under `test/`.

## Coding Style & Naming Conventions
- Indentation uses tabs in `src/` (follow existing formatting).
- Braces are on the same line as control statements/functions (K&R style).
- Prefer existing naming patterns: `UPPER_SNAKE_CASE` for macros, `PascalCase` for system init helpers, and descriptive, short identifiers for registers and pins.
- No formatter or linter is configured; avoid mass reformatting.

## Testing Guidelines
- Place tests under `test/` and keep them focused on single modules.
- Follow PlatformIO Test Runner conventions (see `test/README`).
- Add tests for new drivers or timing-sensitive logic where feasible.

## Commit & Pull Request Guidelines
- This workspace does not include Git history, so no established commit style is visible.
- Use short, imperative commit subjects (e.g., "Add UART init for debug") and include context in the body when needed.
- In PRs, describe the hardware target, link related issues, and include evidence (log output, scope captures, or photos) when behavior changes.

## Hardware & Configuration Notes
- Build environments are defined in `platformio.ini` and use `minichlink` for upload/debug.
- Pin assignments and project notes are summarized in `projectinfo.md`.
