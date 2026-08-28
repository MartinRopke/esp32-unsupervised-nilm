# Smart-plug reference: DPS discovery and update cadence

Capture date: 27 Aug 2026

First bench session with the Tuya smart plug that will serve as the reference
for Stage 4 (apparent-energy / cost validation against the ESP32 estimate).
This session does **not** validate anything against the ESP32 yet; it
establishes what the plug exposes on the local network, how to read it, and how
fast it updates, so the Stage 4 comparison can be designed on facts instead of
assumptions.

Tool: `tools/smart-plug/collect.py` over `tinytuya` 1.20.0, polling the plug at
192.168.15.6 on the local LAN. No cloud traffic after the one-time
`tinytuya wizard` that extracted the local key. Raw samples in
`docs/measurements/smart-plug-reference.csv`.

## Device

| field | value |
|---|---|
| name | Tomada Inteligente Wi-Fi PLUG BR (Novadigital) |
| model | PLUG-BR |
| product_id | `upcy2nmql3wok6nn` |
| Tuya protocol | 3.5 |
| app / cloud | Smart Life account linked to a free Tuya IoT cloud project (Western America DC) |

## DPS layout (confirmed on this unit)

The cloud device mapping and the observed values agree:

| quantity | DPS | code | raw unit | raw / N = SI | N |
|---|---|---|---|---|---|
| voltage | 20 | `cur_voltage` | decivolts | V | 10 |
| current | 18 | `cur_current` | milliamps | A | 1000 |
| power | 19 | `cur_power` | deciwatts | W | 10 |
| switch | 1 | `switch_1` | bool | -- | -- |
| incremental energy | 17 | `add_ele` | Wh (cloud scale 3) | see below | -- |
| calibration coefficients | 22-25 | `*_coe` | -- | static, not used | -- |

**DPS 17 (`add_ele`) is present but not a usable running energy total.** It was
absent from every response in the earlier windows, then present and pinned at 53
(cloud scale 3 -> 0.053 kWh) for the whole final window -- unchanged through a
full ~800 W sandwich-maker on-cycle. `add_ele` is Tuya's *incremental* energy
counter (consumption since the last report, reset on report), not an odometer,
and it does not refresh at a rate useful for a short bench run. Consequence for
Stage 4: apparent energy (VA.h) must be integrated from the logged `current` /
`voltage` / `power` samples, not read from this counter. Option A of the open
validation decision does not need a counter, so this does not block it.

## Cross-check against the reference load

The sandwich maker used as the calibration load elsewhere in this project has a
directly measured hot resistance of **70.85 Ohm** (`reference-load-resistance.md`,
+/-1.2%). During its steady on-phase the plug reported:

| plug reading | value | expected from 70.85 Ohm at the plug's own 237.2 V | difference |
|---|---|---|---|
| current | 3.343 A | 3.348 A | -0.15% |
| power | 794.9 W | 795.0 W (V^2 / R) | -0.01% |
| voltage | 237.2 V | -- (mains, taken as given) | -- |

The plug agrees with the independently measured load resistance to better than
0.2%, well inside that measurement's own 1.2% uncertainty. The plug was not
calibrated against a traceable standard here, so this is an agreement check, not
a calibration; but it establishes the plug as a credible reference, not merely a
convenient one. `power` also equals `voltage x current` to under 0.3% at every
steady point, i.e. the plug's own power figure is consistent with treating this
resistive load as unity power factor.

## Update cadence

The plug does **not** stream at the poll rate. Behaviour observed over four
on/off transitions (two under `probe`, two in the logged session):

- **Steady plateau:** no updates at all. The 36-sample on-phase in the CSV
  reads an identical 794.9 W / 3.343 A / 237.2 V on every sample. The values
  are correct, just not refreshed while nothing changes. Idle voltage likewise
  held 240.9 V unchanged for minutes.
- **On a step change:** the plug pushes a partial DPS dict containing only the
  keys that moved (e.g. `{"20":2372,"18":3343,"19":7949}`), within ~1 s of the
  actual load change. On a clean resistive step all three DPS appear in the
  same push and are mutually consistent.
- **Transient disagreement:** in one off-transition during `probe`, `cur_power`
  briefly lagged `cur_current` by a few seconds (read 615 W while V x I was
  ~800 W) before settling. Each DPS updates on its own schedule; expect ~1-2 s
  of edge ambiguity per event.

`collect.py log` carries each value forward across a partial-dict poll that
omits its DPS (an omitted key means unchanged), so the CSV columns stay
populated; the `raw_dps` column preserves exactly what arrived, partial or full.

### Effect on the Stage 4 comparison

Fine for **per-cluster apparent energy on steady plateaus**, which is what the
hypothesis is about: the plug delivers correct V/I/P there. The limitation is
**~1-2 s of timing ambiguity at each on/off edge**, plus the plug's own
quantisation (0.1 V, 1 mA, 0.1 W). Both belong in the TCC text as
**reference-side** limitations, not ESP32-side ones. They do not affect the
ESP32 number, only the resolution at which the reference can contradict it.

## What this session does not do

- **Does not validate any ESP32 output.** No simultaneous ESP32 capture was
  taken; that is the actual Stage 4 session.
- **Does not calibrate the plug.** The reference-load cross-check is an
  agreement check within the load resistance's own uncertainty, not a
  calibration against a traceable standard.
- **Does not characterise the plug under a non-resistive load.** Power factor
  behaviour (whether `cur_power` tracks real power under a reactive or
  switching load) is untested; the only load used was the resistive sandwich
  maker.
- **Does not establish long-term drift** or behaviour across a re-pair (the
  local key would change on a re-pair; it was stable this session).
- **Does not pin down `add_ele` semantics.** It was seen present-and-constant
  and also absent; the "incremental, resets on report" reading is the standard
  Tuya meaning, not something confirmed by watching a reset here.
- **Touches nothing under `src/` or `lib/`.** This is a bench tool only.

## Effect on the open validation decision (VA.h vs R$)

`NOTAS-PARA-O-TCC.md`, "DECISAO EM ABERTO", left the validation design hanging
on one fact: does the plug export instantaneous current/voltage/power, or only
kWh? **Answer: it exports instantaneous V/I/P locally (DPS 20/18/19), and does
NOT export a usable local kWh counter.** This is exactly the case that makes
**option A** cheap: log the DPS with our own timestamp, integrate VA.h directly,
no dependence on the plug's power factor or any app export. The decision can be
closed in favour of option A on this basis.
