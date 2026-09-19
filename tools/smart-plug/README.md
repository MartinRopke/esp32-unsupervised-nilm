# Smart-plug reference logger

Bench tool for validation runs. One Tuya smart plug per test appliance
(sandwich maker, charger, fan) is the reference the ESP32 per-cluster estimate
is checked against; this script polls the plugs over the **local network** (no
cloud after first-time setup) and writes a CSV per plug with the same
time-column convention as the other runs in `docs/measurements/`. A validation
run needs all three logging at once (`log-all`), alongside the ESP32 serial
capture of the aggregate.

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

Answer with the Cloud Project **Access ID** and **Access Secret** (held
privately, never committed), region **`us`** (Western America DC = Brazil). The
wizard pulls the device list from the cloud once and writes `devices.json`
containing each plug's `local_key`. After this, nothing here needs the cloud.

`devices.json`, `tinytuya.json` and `snapshot.json` are git-ignored: the local
key is an access credential for the device. It does not change unless the plug
is re-paired, so this step is done once, not per session. Pair every plug in
the Smart Life app and link all of them to the Cloud Project before running the
wizard, so one `devices.json` holds all three.

### Name the plugs by appliance (`loads.json`)

The wizard names the plugs generically. Copy `loads.example.json` to
`loads.json` and map each plug (device id, or its exact name from
`devices.json`) to a short label: `sanduicheira`, `carregador`, `ventilador`.
`log-all` uses these to name each CSV. `loads.json` is git-ignored (the pairing
is specific to one bench).

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

All three plugs are the same `product_id` and protocol and return the same DPS
keys. Cross-check each plug under a known load before trusting `log-all`'s
output for it; see the code comments in `collect.py` for the energy column's
known quirk and the partial-update behavior to expect between polls.

### 3. Log a session

All plugs at once, for a validation run:

```sh
python collect.py log-all --out-prefix session-1 --echo
```

Writes `session-1-sanduicheira.csv`, `session-1-carregador.csv`,
`session-1-ventilador.csv` (labels from `loads.json`). One thread per plug,
one shared start instant, so the `t_s` column lines up across the three files
without post-hoc clock alignment. One Ctrl-C stops all of them; `--duration N`
ends the run after N seconds. It refuses to start if any target CSV already
exists, so a re-run does not silently append to the previous session.

A single plug (for probing or a one-load check):

```sh
python collect.py log --device "<name>" --out session-1.csv --echo
```

Both poll at 1 Hz and do not force `updatedps()` (that costs ~5 s/poll on
protocol 3.5 and is unnecessary since the plug pushes changes itself). Pass
`--refresh` only if you suspect a value has gone stale with no push.

If a future unit uses different DPS, override the map (applies to every plug in
`log-all`):

```sh
python collect.py log-all --out-prefix session-1 --echo \
  --map '{"voltage_v":{"dps":"21","scale":10},"current_a":{"dps":"22","scale":1000},"power_w":{"dps":"23","scale":10}}'
```

## Output columns

```
iso_time,t_s,voltage_v,current_a,power_w,energy_kwh,raw_dps
```

- `iso_time`: wall clock, for aligning with the ESP32 serial capture
- `t_s`: seconds since the logger started, matching the `t_s` column in
  `docs/measurements/event-detection-session-*.csv` etc.
- `voltage_v` / `current_a` / `power_w`: mapped DPS values; a value is carried
  forward across a partial-dict poll that omits its DPS (an omitted key means
  unchanged). `--raw-only` disables the carry-forward and leaves the cell blank.
- `raw_dps`: the DPS dict exactly as received (may be partial), so the mapping
  and the carry-forward can be re-derived later without re-running the bench

## Where sessions live

Committed session CSVs go under `docs/measurements/` alongside the other
bench data (e.g. `smart-plug-reference.csv`), following the existing pattern.
For a validation run, commit all three per-plug CSVs plus the ESP32 capture from
the same window.
