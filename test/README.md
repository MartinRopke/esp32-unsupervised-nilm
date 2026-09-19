Native unit tests for the lib/ modules (PlatformIO Unity, `native` platform, no ESP32
board or Arduino dependency required):

```sh
pio test -e native
```

- `test_meter/` - Meter (DC-offset removal, RMS windowing, current and power conversion).
- `test_event_detector/` - EventDetector (threshold-crossing detection with confirmation window).
- `test_event_merger/` - EventMerger (fusing same-direction fragments, release window).
- `test_event_clusterer/` - EventClusterer (1D DBSCAN over event magnitudes).
- `test_cluster_report/` - ClusterReport (pairing on/off events into cycles, per-cluster report).
- `test_session_csv/` - SessionCsv (formatting one CSV row of the bench-session log).

More on the PlatformIO test runner:
https://docs.platformio.org/en/latest/advanced/unit-testing/index.html
