# Smart-plug reference logger

Bench tool for Stage 4 validation. The Tuya smart plug is the reference the
ESP32 apparent-energy estimate is checked against; this script polls the plug
over the **local network** (no cloud after first-time setup) and writes a CSV
with the same time-column convention as the other runs in `docs/measurements/`.

Not firmware. Runs on the laptop, on the same LAN as the plug.

## One-time setup

```sh
cd tools/smart-plug
python3 -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt
```

### 1. Get the local key (`devices.json`)

```sh
python -m tinytuya wizard
```

Answer with the Cloud Project **Access ID** and **Access Secret** (held by
Martin, never committed), region **`us`** (Western America DC = Brazil). The
wizard pulls the device list from the cloud once and writes `devices.json`
containing each plug's `local_key`. After this, nothing here needs the cloud.

`devices.json`, `tinytuya.json` and `snapshot.json` are git-ignored: the local
key is an access credential for the device. It does not change unless the plug
is re-paired, so this step is done once, not per session.

If `devices.json` ends up with an empty `ip`, run a LAN scan with the plug
powered and connected:

```sh
python -m tinytuya scan
```

then re-run the wizard (or paste the IP into `devices.json` by hand).

### 2. Identify the DPS for voltage, current, power

DPS codes and scale factors vary by model/firmware, so read them off this unit
rather than assuming. Put the plug under a real, changing load and watch:

```sh
python collect.py probe
```

Vary the load (switch appliances on/off, use a resistive load you can compute).
Note which DPS keys track voltage, current and power, and infer each scale
factor from a known operating point, e.g. a DPS reading `2203` at ~220 V means
`scale = 10` (decivolts).

Confirmed layout for the unit on the bench (Novadigital PLUG-BR, `product_id`
`upcy2nmql3wok6nn`, protocol 3.5), which is also the `collect.py` default:

| quantity | DPS | code | raw unit | scale (raw/scale = SI) |
|---|---|---|---|---|
| voltage  | 20 | cur_voltage | decivolts | 10 |
| current  | 18 | cur_current | milliamps | 1000 |
| power    | 19 | cur_power   | deciwatts | 10 |
| energy (cumulative) | 17 | add_ele | Wh | 1000 |

Checked against the sandwich-maker reference load: DP18 3357 / DP19 8015 /
DP20 2377 = 3.357 A / 801.5 W / 237.7 V, and V x I (798 W) agrees with DP19 to
under 1%. **DP17 (add_ele) is present intermittently and does not track energy
in real time** -- it stayed pinned at 53 (~0.053 kWh) through a full ~800 W
on-cycle. It is Tuya's incremental counter (resets on report), not an odometer,
so integrate VA.h from the V/I/P samples. `collect.py` still logs the column
(it is informative when present); pass `--no-energy` to drop it.

**Update cadence — this unit.** The plug does not stream at the poll rate; it
pushes a partial DPS dict (only the keys that moved) when a reading changes,
otherwise `status()` just returns the last values. On a clean resistive step it
reflects the change within ~1 s and all three DPS update together and stay
mutually consistent. At a steady plateau nothing updates for tens of seconds --
the values are correct, just not refreshed. Within the first second or two of a
transition the three DPS can briefly disagree (one lagging). Net effect on the
Stage 4 comparison: fine for per-cluster VA.h on steady plateaus, ~1-2 s of edge
ambiguity per on/off event, which belongs in the writeup as a reference-side
limitation, not an ESP32 one.

### 3. Log a session

Defaults match this unit, so:

```sh
python collect.py log --out session-1.csv --echo
```

Polls at 1 Hz; it does not force `updatedps()` (that costs ~5 s/poll on
protocol 3.5 and is unnecessary since the plug pushes changes itself). Pass
`--refresh` only if you suspect a value has gone stale with no push.

If a future unit uses different DPS, override the map:

```sh
python collect.py log --out session-1.csv --echo \
  --map '{"voltage_v":{"dps":"21","scale":10},"current_a":{"dps":"22","scale":1000},"power_w":{"dps":"23","scale":10}}'
```

## Output columns

```
iso_time,t_s,voltage_v,current_a,power_w,energy_kwh,raw_dps
```

- `iso_time` — wall clock, for aligning with the ESP32 serial capture
- `t_s` — seconds since the logger started, matching the `t_s` column in
  `docs/measurements/event-detection-session-*.csv` etc.
- `voltage_v` / `current_a` / `power_w` — mapped DPS values; a value is carried
  forward across a partial-dict poll that omits its DPS (an omitted key means
  unchanged). `--raw-only` disables the carry-forward and leaves the cell blank.
- `raw_dps` — the DPS dict exactly as received (may be partial), so the mapping
  and the carry-forward can be re-derived later without re-running the bench

## Where sessions live

Committed session CSVs and the writeup go under `docs/measurements/` alongside
the other bench data (e.g. `smart-plug-reference.csv` /
`smart-plug-reference.md`), following the existing pattern.
