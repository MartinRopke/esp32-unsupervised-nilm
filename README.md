# esp32-unsupervised-nilm

Firmware for an unsupervised Non-Intrusive Load Monitoring (NILM) system on
the ESP32. It reads a single aggregate current-transformer signal, detects
appliance on/off switching events from that signal, and groups those events
into per-appliance clusters at runtime with no signature database and no
labelled training data, then reports per-cluster operating time, apparent
energy and an estimated cost.

This is the firmware and reference tooling behind an MBA thesis (TCC) in
Software Engineering at USP/ESALQ; see "Citing this work" below.

## Hardware

- ESP32 board (PlatformIO `esp32dev`, Arduino framework).
- ADS1115 16-bit ADC (`adafruit/Adafruit ADS1X15` library), read continuously
  in differential mode (A0-A1) at 860 SPS over I2C at 400 kHz.
- SCT-013-000 current transformer, with a burden resistor across the ADC
  input, feeding the ADC the current signal as a burden voltage.
- Mains voltage and the CT/burden calibration chain (turns ratio, burden
  resistance, an empirical calibration factor) are installation constants set
  in `src/main.cpp`, not measured by the firmware itself.
- Serial output at 921600 baud, used both for live bench monitoring and for
  the CSV session log.

## Repository layout

- `lib/` - the six pure, Arduino-independent modules that make up the NILM
  pipeline (measurement, event detection, event merging, clustering, cluster
  reporting, CSV formatting). See `lib/README` for what each one does.
- `src/` - the firmware entry point (`main.cpp`, which wires the `lib/`
  modules together and holds the installation configuration) plus the
  Arduino-only Serial output wrappers (`teleplot_output`, `session_csv_output`,
  `cluster_report_output`) around the pure formatters in `lib/`.
- `test/` - PlatformIO/Unity native unit tests for the `lib/` modules.
- `docs/measurements/` - raw measurement/bench-characterization data (CSV).
- `tools/smart-plug/` - a reference power logger that reads Tuya smart plugs
  over the local network, used as ground truth against the ESP32's estimate
  during bench validation. See `tools/smart-plug/README.md` for setup; the
  device/credential files it writes (`devices.json`, `tinytuya.json`,
  `snapshot.json`, `loads.json`, etc.) are git-ignored on purpose and are not
  described here.
- `include/` - PlatformIO's default project header folder; unused, since
  every header in this project lives next to its module under `lib/` or
  `src/`.

## Building and flashing

This is a PlatformIO project. With the PlatformIO CLI (or the VS Code
PlatformIO extension) installed:

```sh
pio run -e esp32dev -t upload
```

`esp32dev` is also the default environment, so a plain `pio run -t upload`
does the same thing. Monitor the serial output (921600 baud) with:

```sh
pio device monitor -b 921600
```

## Running the unit tests

The `native` environment builds and runs the `lib/` unit tests on the host,
with no ESP32 board and no Arduino dependency (it compiles only `lib/` and
`test/`, excluding everything under `src/`):

```sh
pio test -e native
```

## Code style

Code is formatted with `clang-format` (see `.clang-format`, based on Google's
style with a 100-column limit). `requirements-dev.txt` pins the `clang-format`
version used.

## License

MIT License, see `LICENSE`.

## Citing this work

Martin Ropke, source code for an MBA thesis (TCC) in Software Engineering at
USP/ESALQ on unsupervised Non-Intrusive Load Monitoring with an ESP32, 2026.
[link to the published thesis to be added here]
