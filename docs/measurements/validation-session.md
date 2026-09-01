# Stage 4 validation session — apparent energy and cost per cluster

Joint capture of the ESP32 firmware against the three smart plugs, run to
produce the title metric of the work: percentage error of apparent energy
(VA·h) per cluster, ESP32 versus the plug that monitors the same appliance,
with cost (R$) as the secondary metric.

Capture date: 31 Aug 2026, 22:45–23:25 BRT.
Firmware: commit `21fe27c` (repository HEAD at session time; flashed in the
prior bench session, not reflashed for this one). No `lib/` or `src/` change
in scope. Serial config as committed: `kEnableCsvOutput = true`,
`kEnableTeleplotOutput = false`.

Reference tool: `tools/smart-plug/collect.py`, mode `log-all`, commit
`21fe27c`, run with `--refresh` (forces `updatedps()` before every poll;
without it the plugs freeze their last reading for tens of seconds to
minutes on protocol 3.5). Effective poll rate ~1 sample per 5 s per plug.

Multimeter: mains voltage measured once at a free socket on the sacrificial
strip, everything off: **239.8 V~**. The firmware computes power from a fixed
`mainsVoltage = 236.75 V` constant, so ESP32 apparent power carries a
~1.3 % low bias from the voltage constant alone, before any current-path
error.

## Rig

Three appliances — box fan (`ventilador`), laptop charger (`carregador`),
sandwich maker (`sanduicheira`) — each on its own Novadigital PLUG-BR smart
plug (protocol 3.5), the three plugs on the sacrificial power strip whose
single conductor carries the SCT-013 current transformer. The CT sees the
aggregate of the three; each plug sees only its own appliance. **No other
load was on the strip** — only the three test plugs and their appliances.

Plug↔appliance mapping (`tools/smart-plug/loads.json`, verified 31 Aug by
differential toggle) and plug IPs (`tools/smart-plug/devices.json`, refreshed
by `tinytuya scan` after DHCP drift): sanduicheira 192.168.15.12, carregador
192.168.15.13, ventilador 192.168.15.9.

## Session start and clock alignment

| stream | start (wall clock) | note |
|---|---|---|
| ESP32 serial capture | 22:45:44.868 `# START` line | port opened ~22:45:41.8; board resets on port open, so firmware `t_s = 0` ≈ 22:45:41.8 |
| `collect.py log-all` | 22:45:46.37 (`t0`) | 1.50 s after the serial `# START` line, ~4.5 s after firmware `t_s = 0` |

**Measured start delay: 1.50 s** (serial capture began first). The firmware
`t_s` clock runs within 0.2 % of wall-clock over the session
(2334 telemetry rows across 2341 `t_s` seconds ≈ 2339 s wall), so
`wall ≈ 22:45:41.8 + t_s` holds throughout; plug `iso_time` aligns to that.

### Synchronisation marker

Fan switched on, held ~15 s, switched off, everything else off:

| edge | wall clock (`# MARK`) | firmware `event_t_s` | Δ apparent power |
|---|---|---|---|
| fan on | 22:51:03.073 | 315.02 | +49.1 VA |
| fan off | 22:51:20.074 | 331.02 | −33.9 VA |

The ~14 s pulse is clean in the ESP32 series. **The plugs did not register
it** — 14 s is shorter than the ~5 s plug poll can resolve as a plateau, the
same limitation that set the 60–75 s dwell for the load blocks. Cross-series
alignment therefore rests on the ESP32 series plus the `# MARK` wall clocks
plus plug `iso_time`, not on a four-way-visible pulse.

## Session timeline

All `# MARK` lines are in `validation-session-esp32.csv` with wall clock and
`perf_counter`.

| segment | `# MARK` wall clock | firmware `event_t_s` of the edges |
|---|---|---|
| connection-artifact check, sandwich on | 22:47:14 | on 84.02 |
| connection-artifact check, sandwich off | 22:49:06 | off 194.02 |
| sync marker (fan on / off) | 22:51:03 / 22:51:20 | 315.02 / 331.02 |
| ventilador block start / end | 22:54:36 / 23:00:48 | on/off 536/618, 661/741, 782/863 |
| carregador block start / end | 23:02:21 / 23:08:23 | on/off 1002/1083, 1127/1201, 1246/1320 |
| sanduicheira block start / end | 23:13:11 / 23:18:51 | on/off 1650/1731, 1773/1835, 1885/1919 |
| simultaneous ventilador + carregador on / off | 23:21:50 / 23:23:28 | 2156 / 2256 |
| console report (`r`) requested | 23:23:57 | — |
| CSV report (`c`) requested | 23:24:01 | — |
| serial capture stopped | 23:24:46 | — |

Each load block was three cycles. Dwell: ventilador ~75 s on / ~30 s off;
carregador ~75 s on / ~30 s off; sanduicheira 60 s on / ~40 s off (shortened
from the fan/charger dwell to stay clear of the thermostat). The handoff's
10 s minimum was not used — the plugs miss cycles that short on protocol 3.5.

Charger battery: 16 % at block start, 20 % at block end.

**Duration:** serial capture 39 min 1 s (22:45:45 → 23:24:46); load session
proper (first check action to `c` report) ~36.5 min. Peak firmware `t_s`
2344 s, well inside the ~4295 s `micros()` wrap ceiling.

## Connection-artifact check

Idle baseline then sandwich maker conducting, per the standing procedure.

| window | source | n | mean | rel. std dev |
|---|---|---|---|---|
| idle (`t_s` 10–80) | ESP32 apparent power | 68 | 4.20 VA | 0.9 % (abs sd 0.038 VA) |
| sandwich plateau (`t_s` 95–190) | ESP32 apparent power | 95 | 791.1 VA | **0.505 %** (0.32 % after removing the element's −0.11 VA/s thermal drift) |
| sandwich plateau, same window | plug `power_w` | 18 | 790.0 W | 0.0 % (quantised to a single reported value) |

**PASS.** 0.505 % is inside the clean band (0.3–1 %), far from the 4.4–4.8 %
oscillation that flags a bad splice. ESP32 791.1 VA vs plug 790.0 W agree to
0.14 %.

## Result — firmware cluster report

Verbatim in `validation-session-report.txt` (console) and
`validation-session-report.csv`. Tariff R$ 0.9200/kWh (declared constant).
`kClusterPowerFactorAssignments` is empty, so the firmware prints a cost
*range* over |PF| 0.47–1.00 rather than a point cost.

| cluster | appliance | cycles | operating time | apparent energy | firmware cost range |
|---|---|---|---|---|---|
| 1 | ventilador | 4 | 00:04:19 (259 s) | 3.0381 VA·h (+ 16.30 VA·h truncated, misattributed — see below) | R$ 0.0013 – 0.0028 |
| 2 | carregador | 1 | 00:01:20 (81 s) | 2.1769 VA·h | R$ 0.0009 – 0.0020 |
| 3 | sanduicheira | 4 | 00:04:46 (287 s) | 62.7957 VA·h | R$ 0.0272 – 0.0578 |

Cycle counts include the pre-block actions: cluster 1's 4th cycle is the sync
marker; cluster 3's 4th cycle is the connection-artifact check conduction.
Full event→cycle attribution in `validation-session-cluster-map.csv`.

### Secondary cost with Hannagan et al. (2023) power factor

`cost = apparent_energy_vah × |PF| / 1000 × 0.92`. PF and source per
`validation-session-cluster-map.csv` (Hannagan et al. 2023, *Sustainability*
15(1):158):

| cluster | appliance | PF | apparent energy | cost |
|---|---|---|---|---|
| 1 | ventilador | 0.94 | 3.0381 VA·h | R$ 0.0026 |
| 2 | carregador | 0.55 | 2.1769 VA·h | R$ 0.0011 |
| 3 | sanduicheira | 0.99 | 62.7957 VA·h | R$ 0.0572 |

## Cross-comparison, ESP32 apparent power vs plug (plateau means)

| appliance | ESP32 (VA) | plug real power (W) | plug V×I (VA) | ESP32 vs plug V×I |
|---|---|---|---|---|
| ventilador | 47.4 | 46.0 | 45.1 | +5.2 % |
| carregador | 105.2 | 65.9 | 118.0 | −10.8 % |
| sanduicheira | ~793 | ~797 | ~790 | +0.4 % |

The resistive sandwich maker agrees to well under 1 %. The fan reads ~5 %
high. The charger — a switched-mode supply with a distorted current
waveform — reads ~11 % low against the plug's V×I: the SCT-013 + 22 Ω burden
+ ADS1115 chain (860 SPS, 1 s RMS window) under-captures the harmonic content
that the plug's dedicated energy IC resolves. This is a real limitation of
the current front end on non-linear loads, not a wiring fault.

## What went wrong, without dressing it up

- **The charger cluster is badly underreported.** Its switched-mode
  signature spread the six block events across Δ 55.9–101.1 VA. DBSCAN
  (`epsilonVa` 12, `minPoints` 4) kept four events near ~97 VA — one on and
  three offs — so cluster 2 reports **1 complete cycle (81 s) against a
  ground truth of 3 cycles (~205 s)**, roughly a 60 % underreport of
  operating time and apparent energy. Two switch-ons were lost: Δ 69.8 VA
  dropped as noise, Δ 55.9 VA absorbed by the fan cluster.
- **The fan cluster carries a 16.30 VA·h phantom "truncated cycle".** That
  Δ 55.9 VA charger switch-on (event_t_s 1246) chained onto cluster 1
  (55.9 − 49.1 = 6.8 VA < `epsilonVa`) and never paired, so it sits open to
  end of capture. The fan's own energy is the 3.0381 VA·h from its four clean
  pairs; the truncated tail is misattributed charger.
- These two are the same event: one appliance's messy signature leaking into
  a neighbouring cluster by magnitude. No parameter was changed to hide it
  (out of scope: `epsilonVa`, `minPoints`, `mergeWindowSeconds`, pairing).
  The clean single-appliance clusters (fan, sandwich) reproduce their ground
  truth exactly on operating time (259 s and 287 s, to the second).
- **Sandwich block cycle 3 ran only ~34 s** (thermostat cut early — element
  still warm from cycles 1–2). It still paired cleanly; the reference VA·h
  for that cycle rests on ~6 plug samples.
- **Sandwich block cycle 2: the plug froze its current DP** (`I` stuck at
  0.001 A) while still reporting `power_w` = 796 W. Use `power_w` for that
  cycle, not V×I.
- **Plug missed the 14 s sync-marker fan pulse** entirely (poll too slow for
  a pulse that short). Alignment does not depend on it.
- **8 corrupted serial frames in 39 min** (~0.3 % of rows), single lines,
  `t_s` continuous on both sides — not reboots.
- **The ESP32 rebooted repeatedly (`POWERON_RESET`, every 1.5–4 min) in the
  runs that preceded this session.** Root cause: a breadboard capacitor whose
  leads were touching, an intermittent short on the supply rail. After that
  was fixed the board ran an 11 min idle proof and this full 39 min session
  with zero resets and monotonic `t_s`. Two earlier session attempts on
  31 Aug were discarded to this fault (`scratchpad` incident notes, not
  committed). One further attempt earlier on 31 Aug was discarded to the
  plugs freezing readings before `--refresh` was made mandatory.

## What this session does not settle

- It does not fix or assess the charger clustering failure — it records it.
  Whether Δ-magnitude clustering is the right primitive for switched-mode
  loads is a design question, not something this capture decides.
- It does not measure power factor. The Hannagan PF values are table
  look-ups by appliance category; the firmware's own output is a cost range,
  not a PF.
- It does not characterise the current front end's harmonic response beyond
  the single −10.8 % data point on this one charger at this one battery
  state.
- The `micros()` 32-bit wrap at ~4295 s was not reached and is not addressed
  here; a 64-bit time base for long sessions remains a separate item.

## Artifacts

| file | content |
|---|---|
| `validation-session-ventilador.csv` | plug log, fan, direct `collect.py log-all` output |
| `validation-session-carregador.csv` | plug log, charger |
| `validation-session-sanduicheira.csv` | plug log, sandwich maker |
| `validation-session-esp32.csv` | ESP32 serial log (telemetry rows + `# MARK`/`# SEND`/`# START`/`# STOP`; `r`/`c` report bodies removed to the report files) |
| `validation-session-report.txt` | console cluster report, verbatim |
| `validation-session-report.csv` | CSV cluster report, verbatim |
| `validation-session-cluster-map.csv` | cluster → appliance, event-by-event attribution, Hannagan PF |
| `validation-session.md` | this file |

Figure 8 of the `.docx` (photo of the full rig — three appliances, three
plugs, strip, CT) is still to be taken; without it the placeholder in
Material e Métodos stays empty.
