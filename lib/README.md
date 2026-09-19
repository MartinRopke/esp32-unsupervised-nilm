Project-specific libraries for the NILM pipeline.

PlatformIO compiles each subdirectory here into a static library and links it
into the firmware (`src/main.cpp`). All six modules are plain C++ with no
Arduino, Wire or Serial dependency, so they also build and are unit tested in
the `native` environment (`pio test -e native`), independently of the ESP32
board.

- `meter/` - Turns one raw burden-voltage sample into apparent power. Removes
  the DC offset with an exponential moving average, accumulates a time-closed
  RMS window over the burden voltage, and converts the window's RMS voltage
  into RMS current (via the CT turns ratio, burden resistance and an
  empirical calibration factor) and then into apparent power.

- `event_detector/` - Fed one apparent-power sample per closed Meter window,
  detects appliance on/off events as threshold-crossing steps in that series,
  confirming a candidate transition over a fixed window of samples before
  reporting it.

- `event_merger/` - A single switching transition can reach the detector as
  several same-direction fragments spread over a few sampling windows. This
  module fuses consecutive same-direction fragments into one event and
  releases it once no further fragment arrives within the merge window.

- `event_clusterer/` - Fed merged events' magnitudes, maintains a bounded
  history and assigns each one a one-dimensional DBSCAN cluster label,
  re-clustering the full retained history on every call. Discovers the number
  of active appliances on its own, with no signature database.

- `cluster_report/` - Pairs on/off events within each cluster into
  on/off cycles and turns them into a per-cluster operating report: cycle
  count, operating time, mean apparent power, apparent energy, and an
  estimated cost from a human-supplied power-factor category.

- `session_csv/` - Pure formatter for one row of the bench-session CSV log
  (time, RMS voltage/current, power, and, when one closed on that row, the
  merged event and its cluster).
