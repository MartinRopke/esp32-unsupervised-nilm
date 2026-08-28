"""Poll Tuya smart plugs on the local network and log V / I / P to CSV.

This is a bench reference tool for Stage 4 validation, not ESP32 firmware. The
smart plugs are the reference the ESP32 estimate is checked against; see
docs/measurements/ for the column convention shared with the other bench runs.

Prerequisites (run once, see README.md):
  1. pip install -r requirements.txt
  2. python -m tinytuya wizard      writes devices.json (holds local_key)
  3. python collect.py probe        identify which DPS carry V, I, P

Three subcommands:
  probe    dump the full DPS dictionary every poll, so you can watch which keys
           move when the load changes and read off their scale factors
  log      map the chosen DPS to voltage / current / power columns, one plug
  log-all  same as log but every plug in devices.json at once, one CSV each,
           sharing a single start instant so t_s lines up across the files

No subcommand touches the cloud once devices.json exists: all traffic is local
to each plug's IP.
"""

import argparse
import csv
import datetime as dt
import json
import pathlib
import re
import sys
import threading
import time

HERE = pathlib.Path(__file__).parent
DEVICES_JSON = HERE / "devices.json"
# Optional map of device id (or name) to a short appliance label, used to name
# the per-plug CSVs in log-all. Kept out of version control like devices.json;
# loads.example.json shows the shape.
LOADS_JSON = HERE / "loads.json"

# Codes seen on most Tuya energy-monitoring plugs running protocol 3.3. They are
# NOT guaranteed for a given unit, confirm with `probe` before trusting `log`.
# scale is the divisor from the raw integer DPS value to the SI unit.
DEFAULT_MAP = {
    "voltage_v": {"dps": "20", "scale": 10.0},   # raw decivolts  -> V
    "current_a": {"dps": "18", "scale": 1000.0},  # raw milliamps  -> A
    "power_w":   {"dps": "19", "scale": 10.0},    # raw deciwatts  -> W
}
# Optional cumulative-energy DPS. On this unit (DP 17 add_ele) the cloud mapping
# gives scale 3, i.e. raw is in Wh; step 100 means 0.1 kWh resolution, too coarse
# for short runs, integrate power/current from the samples instead, this is a
# long-run sanity check only.
DEFAULT_ENERGY = {"dps": "17", "scale": 1000.0}   # raw Wh -> kWh


def _slug(text):
    return re.sub(r"[^a-z0-9]+", "-", str(text).lower()).strip("-")


def read_devices():
    if not DEVICES_JSON.exists():
        sys.exit(
            f"{DEVICES_JSON.name} not found. Run `python -m tinytuya wizard` "
            "in this folder first (see README.md)."
        )
    return json.loads(DEVICES_JSON.read_text())


def read_load_labels():
    if not LOADS_JSON.exists():
        return {}
    return json.loads(LOADS_JSON.read_text())


def connect(entry):
    """Build a connected OutletDevice from a devices.json entry. Raises on gaps."""
    try:
        import tinytuya
    except ImportError:
        sys.exit("tinytuya not installed. Run: pip install -r requirements.txt")
    if not entry.get("ip"):
        raise RuntimeError(
            f"device {entry.get('name')!r} has no IP in devices.json. Run "
            "`python -m tinytuya scan` with the plug powered and on this LAN, "
            "then re-run the wizard (or paste the IP into devices.json)."
        )
    dev = tinytuya.OutletDevice(
        dev_id=entry["id"],
        address=entry["ip"],
        local_key=entry["key"],
        version=float(entry.get("version", 3.3)),
    )
    dev.set_socketPersistent(True)
    return dev


def load_device(name_or_id):
    """Return (dev, entry) for the single plug matching name_or_id."""
    devices = read_devices()
    match = None
    for entry in devices:
        if name_or_id in (entry.get("name"), entry.get("id")):
            match = entry
            break
    if match is None:
        names = ", ".join(repr(d.get("name")) for d in devices)
        sys.exit(f"No device named/id {name_or_id!r} in devices.json. Have: {names}")
    try:
        return connect(match), match
    except RuntimeError as exc:
        sys.exit(f"! {exc}")


def read_status(dev, refresh_dps):
    """One status read. Optionally force-refresh the given DPS list first."""
    if refresh_dps:
        try:
            dev.updatedps(refresh_dps)
        except Exception:
            pass  # not all firmware supports it; status() still returns cached
    data = dev.status()
    if not isinstance(data, dict) or "dps" not in data:
        return None, data
    return data["dps"], data


def build_specs(args):
    """Resolve the DPS->column map and optional energy spec from args."""
    col_map = json.loads(args.map) if args.map else dict(DEFAULT_MAP)
    energy = None if args.no_energy else DEFAULT_ENERGY
    if args.energy_map:
        energy = json.loads(args.energy_map)
    fields = ["iso_time", "t_s", "voltage_v", "current_a", "power_w"]
    if energy:
        fields.append("energy_kwh")
    fields.append("raw_dps")
    refresh = None
    if args.refresh:
        refresh = sorted({c["dps"] for c in col_map.values()} |
                         ({energy["dps"]} if energy else set()))
    return col_map, energy, fields, refresh


def poll_loop(dev, label, writer, fh, col_map, energy, refresh, interval, t0,
              deadline, stop, echo, raw_only, fatal_misses):
    """Poll one plug until deadline / stop, appending mapped rows to writer."""
    misses = 0
    written = 0
    # The plug pushes partial DPS dicts on change: a poll can return only the
    # keys that just moved. Keys absent from a response are unchanged, so we
    # carry the last seen value forward unless raw_only is set.
    last = {}
    specs = dict(col_map)
    if energy:
        specs["energy_kwh"] = energy
    while not stop.is_set():
        loop_start = time.monotonic()
        if deadline and loop_start >= deadline:
            break
        dps, raw = read_status(dev, refresh)
        now = dt.datetime.now()
        if dps is None:
            misses += 1
            print(f"! [{label}] no dps in response ({misses}): {raw}", file=sys.stderr)
            if misses >= 10:
                if fatal_misses:
                    sys.exit(f"! [{label}] 10 consecutive empty responses, giving up")
                print(f"! [{label}] still no data after {misses} polls, "
                      "keeping the thread alive", file=sys.stderr)
        else:
            misses = 0
            row = {
                "iso_time": now.isoformat(timespec="milliseconds"),
                "t_s": round(loop_start - t0, 3),
                "raw_dps": json.dumps(dps, separators=(",", ":")),
            }
            for col, spec in specs.items():
                v = dps.get(spec["dps"])
                if v is None and not raw_only:
                    v = last.get(spec["dps"])
                else:
                    last[spec["dps"]] = v
                row[col] = round(v / spec["scale"], 4) if v is not None else ""
            writer.writerow(row)
            fh.flush()
            written += 1
            if echo:
                tag = f"[{label}] " if label else ""
                print(f"{tag}{row['t_s']:>8}s  V={row['voltage_v']}  "
                      f"I={row['current_a']}  P={row['power_w']}"
                      + (f"  E={row.get('energy_kwh')}" if energy else ""))
        sleep = interval - (time.monotonic() - loop_start)
        if sleep > 0:
            stop.wait(sleep)
    return written


def cmd_probe(args):
    dev, match = load_device(args.device)
    print(f"# probing {match['name']} @ {match['ip']} (protocol {match.get('version')})")
    print("# columns: iso_time  <dps dict>")
    refresh = [s.strip() for s in args.refresh.split(",")] if args.refresh else None
    deadline = time.monotonic() + args.duration if args.duration else None
    try:
        while True:
            dps, raw = read_status(dev, refresh)
            stamp = dt.datetime.now().isoformat(timespec="milliseconds")
            print(f"{stamp}  {dps if dps is not None else raw}")
            if deadline and time.monotonic() >= deadline:
                break
            time.sleep(args.interval)
    except KeyboardInterrupt:
        print("\n# stopped", file=sys.stderr)


def cmd_log(args):
    dev, match = load_device(args.device)
    col_map, energy, fields, refresh = build_specs(args)

    out = pathlib.Path(args.out)
    new_file = not out.exists()
    print(f"# logging {match['name']} @ {match['ip']} -> {out}")
    print(f"# map: {col_map}" + (f"  energy: {energy}" if energy else ""))
    print(f"# interval {args.interval}s, refresh={bool(refresh)}, Ctrl-C to stop")

    stop = threading.Event()
    with out.open("a", newline="") as fh:
        writer = csv.DictWriter(fh, fieldnames=fields, quoting=csv.QUOTE_MINIMAL)
        if new_file:
            writer.writeheader()
        t0 = time.monotonic()
        deadline = t0 + args.duration if args.duration else None
        try:
            poll_loop(dev, "", writer, fh, col_map, energy, refresh, args.interval,
                      t0, deadline, stop, args.echo, args.raw_only, fatal_misses=True)
        except KeyboardInterrupt:
            pass
    print(f"\n# stopped, wrote {out}", file=sys.stderr)


def cmd_log_all(args):
    devices = read_devices()
    if args.only:
        wanted = set(args.only)
        devices = [d for d in devices
                   if d.get("id") in wanted or d.get("name") in wanted]
        missing = wanted - {d.get("id") for d in devices} - {d.get("name") for d in devices}
        if missing:
            sys.exit(f"! --only names not in devices.json: {', '.join(sorted(missing))}")
    if not devices:
        sys.exit("! no devices to log")

    labels = read_load_labels()
    col_map, energy, fields, refresh = build_specs(args)
    prefix = pathlib.Path(args.out_prefix)

    plans = []
    seen = {}
    for entry in devices:
        label = labels.get(entry.get("id")) or labels.get(entry.get("name")) \
            or _slug(entry.get("name") or entry.get("id"))
        if label in seen:
            sys.exit(f"! two devices resolve to label {label!r}: "
                     f"{seen[label]} and {entry.get('name')}. Fix {LOADS_JSON.name}.")
        seen[label] = entry.get("name")
        out = prefix.with_name(f"{prefix.name}-{label}.csv")
        if out.exists():
            sys.exit(f"! {out} exists, refusing to append across runs. "
                     "Move it or pass a fresh --out-prefix.")
        plans.append((entry, label, out))

    try:
        conns = [(connect(e), label, out) for e, label, out in plans]
    except RuntimeError as exc:
        sys.exit(f"! {exc}")

    print(f"# logging {len(conns)} plug(s), interval {args.interval}s, "
          f"refresh={bool(refresh)}"
          + (f", stop after {args.duration}s" if args.duration else ", Ctrl-C to stop"))
    for _, label, out in conns:
        print(f"#   {label:<16} -> {out}")

    stop = threading.Event()
    handles = []
    files = []
    t0 = time.monotonic()
    deadline = t0 + args.duration if args.duration else None
    for dev, label, out in conns:
        fh = out.open("w", newline="")
        files.append((fh, label, out))
        writer = csv.DictWriter(fh, fieldnames=fields, quoting=csv.QUOTE_MINIMAL)
        writer.writeheader()
        th = threading.Thread(
            target=poll_loop,
            args=(dev, label, writer, fh, col_map, energy, refresh, args.interval,
                  t0, deadline, stop, args.echo, args.raw_only, False),
            name=label,
            daemon=True,
        )
        th.start()
        handles.append(th)

    try:
        while any(th.is_alive() for th in handles):
            time.sleep(0.2)
    except KeyboardInterrupt:
        print("\n# stopping all plugs", file=sys.stderr)
        stop.set()
    for th in handles:
        th.join(timeout=args.interval + 5)
    for fh, label, out in files:
        fh.flush()
        fh.close()
        print(f"# wrote {out}", file=sys.stderr)


def main():
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="cmd", required=True)

    common = argparse.ArgumentParser(add_help=False)
    common.add_argument("--interval", type=float, default=1.0,
                        help="seconds between polls (default 1.0)")
    common.add_argument("--duration", type=float, default=0.0,
                        help="stop after N seconds (default: run until Ctrl-C)")

    mapping = argparse.ArgumentParser(add_help=False)
    mapping.add_argument("--map", default=None,
                         help='JSON overriding the DPS->column map, e.g. '
                              '\'{"voltage_v":{"dps":"20","scale":10},...}\'')
    mapping.add_argument("--energy-map", default=None,
                         help='JSON for the cumulative-energy column, or --no-energy')
    mapping.add_argument("--no-energy", action="store_true",
                         help="do not log a cumulative energy column")
    mapping.add_argument("--raw-only", action="store_true",
                         help="do not carry values forward across partial-dict polls; "
                              "leave a cell blank when its DPS is absent from that poll")
    mapping.add_argument("--refresh", action="store_true",
                         help="force updatedps() before each poll (~5 s/poll on 3.5; "
                              "usually unnecessary, the plug pushes changes itself)")
    mapping.add_argument("--echo", action="store_true",
                         help="print each row as it is written")

    pr = sub.add_parser("probe", parents=[common], help="dump raw DPS every poll")
    pr.add_argument("--device", default=None,
                    help="device name or id in devices.json (default: first entry)")
    pr.add_argument("--refresh", default=None,
                    help="comma-separated DPS to force-refresh before each read")
    pr.set_defaults(func=cmd_probe)

    lg = sub.add_parser("log", parents=[common, mapping],
                        help="write mapped V/I/P CSV for one plug")
    lg.add_argument("--device", default=None,
                    help="device name or id in devices.json (default: first entry)")
    lg.add_argument("--out", default="smart-plug-log.csv", help="output CSV path")
    lg.set_defaults(func=cmd_log)

    la = sub.add_parser("log-all", parents=[common, mapping],
                        help="log every plug in devices.json, one CSV each, shared t0")
    la.add_argument("--out-prefix", default="smart-plug-run",
                    help="each CSV is <prefix>-<label>.csv (default: smart-plug-run)")
    la.add_argument("--only", nargs="+", default=None,
                    help="limit to these device names/ids")
    la.set_defaults(func=cmd_log_all)

    args = p.parse_args()
    if getattr(args, "device", "sentinel") is None and DEVICES_JSON.exists():
        entries = json.loads(DEVICES_JSON.read_text())
        if entries:
            args.device = entries[0].get("name") or entries[0].get("id")
    args.func(args)


if __name__ == "__main__":
    main()
