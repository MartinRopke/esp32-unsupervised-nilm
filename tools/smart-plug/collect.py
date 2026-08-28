"""Poll a Tuya smart plug on the local network and log V / I / P to CSV.

This is a bench reference tool for Stage 4 validation, not ESP32 firmware. The
smart plug is the reference the ESP32 estimate is checked against; see
docs/measurements/ for the column convention shared with the other bench runs.

Prerequisites (run once, see README.md):
  1. pip install -r requirements.txt
  2. python -m tinytuya wizard      -> writes devices.json (holds local_key)
  3. python collect.py probe        -> identify which DPS carry V, I, P

Two subcommands:
  probe   dump the full DPS dictionary every poll, so you can watch which keys
          move when the load changes and read off their scale factors
  log     map the chosen DPS to voltage / current / power columns and write CSV

Neither subcommand touches the cloud once devices.json exists: all traffic is
local to the plug's IP.
"""

import argparse
import csv
import datetime as dt
import json
import pathlib
import sys
import time

HERE = pathlib.Path(__file__).parent
DEVICES_JSON = HERE / "devices.json"

# Codes seen on most Tuya energy-monitoring plugs running protocol 3.3. They are
# NOT guaranteed for this unit -- confirm with `probe` before trusting `log`.
# scale is the divisor from the raw integer DPS value to the SI unit.
DEFAULT_MAP = {
    "voltage_v": {"dps": "20", "scale": 10.0},   # raw decivolts  -> V
    "current_a": {"dps": "18", "scale": 1000.0},  # raw milliamps  -> A
    "power_w":   {"dps": "19", "scale": 10.0},    # raw deciwatts  -> W
}
# Optional cumulative-energy DPS. On this unit (DP 17 add_ele) the cloud mapping
# gives scale 3, i.e. raw is in Wh; step 100 means 0.1 kWh resolution, too coarse
# for short runs -- integrate power/current from the samples instead, this is a
# long-run sanity check only.
DEFAULT_ENERGY = {"dps": "17", "scale": 1000.0}   # raw Wh -> kWh


def load_device(name_or_id):
    """Return a connected OutletDevice for the plug matching name_or_id."""
    try:
        import tinytuya
    except ImportError:
        sys.exit("tinytuya not installed. Run: pip install -r requirements.txt")
    if not DEVICES_JSON.exists():
        sys.exit(
            f"{DEVICES_JSON.name} not found. Run `python -m tinytuya wizard` "
            "in this folder first (see README.md)."
        )
    devices = json.loads(DEVICES_JSON.read_text())
    match = None
    for entry in devices:
        if name_or_id in (entry.get("name"), entry.get("id")):
            match = entry
            break
    if match is None:
        names = ", ".join(repr(d.get("name")) for d in devices)
        sys.exit(f"No device named/id {name_or_id!r} in devices.json. Have: {names}")
    if not match.get("ip"):
        sys.exit(
            f"Device {match.get('name')!r} has no IP in devices.json. Run "
            "`python -m tinytuya scan` with the plug powered and on this LAN, "
            "then re-run the wizard (or paste the IP into devices.json)."
        )

    dev = tinytuya.OutletDevice(
        dev_id=match["id"],
        address=match["ip"],
        local_key=match["key"],
        version=float(match.get("version", 3.3)),
    )
    dev.set_socketPersistent(True)
    return dev, match


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

    col_map = dict(DEFAULT_MAP)
    if args.map:
        col_map = json.loads(args.map)
    energy = DEFAULT_ENERGY if not args.no_energy else None
    if args.energy_map:
        energy = json.loads(args.energy_map)

    fields = ["iso_time", "t_s", "voltage_v", "current_a", "power_w"]
    if energy:
        fields.append("energy_kwh")
    fields.append("raw_dps")

    out = pathlib.Path(args.out)
    new_file = not out.exists()
    # The plug pushes DPS changes on its own, so a plain status() read at the
    # poll rate already catches them. Forcing updatedps() adds a ~5 s round-trip
    # per poll on protocol 3.5 and is only worth it when you suspect a value has
    # gone stale with no push -- opt in with --refresh.
    refresh = (sorted({c["dps"] for c in col_map.values()} |
                      ({energy["dps"]} if energy else set()))
               if args.refresh else None)

    print(f"# logging {match['name']} @ {match['ip']} -> {out}")
    print(f"# map: {col_map}" + (f"  energy: {energy}" if energy else ""))
    print(f"# interval {args.interval}s, refresh={bool(refresh)}, Ctrl-C to stop")

    with out.open("a", newline="") as fh:
        writer = csv.DictWriter(fh, fieldnames=fields, quoting=csv.QUOTE_MINIMAL)
        if new_file:
            writer.writeheader()
        t0 = time.monotonic()
        deadline = t0 + args.duration if args.duration else None
        misses = 0
        # The plug pushes partial DPS dicts on change: a poll can return only the
        # keys that just moved. Keys absent from a response are unchanged, so we
        # carry the last seen value forward unless --raw-only is set.
        last = {}
        try:
            while True:
                loop_start = time.monotonic()
                if deadline and loop_start >= deadline:
                    break
                dps, raw = read_status(dev, refresh)
                now = dt.datetime.now()
                if dps is None:
                    misses += 1
                    print(f"! no dps in response ({misses}): {raw}", file=sys.stderr)
                else:
                    misses = 0
                    row = {
                        "iso_time": now.isoformat(timespec="milliseconds"),
                        "t_s": round(loop_start - t0, 3),
                        "raw_dps": json.dumps(dps, separators=(",", ":")),
                    }
                    specs = dict(col_map)
                    if energy:
                        specs["energy_kwh"] = energy
                    for col, spec in specs.items():
                        v = dps.get(spec["dps"])
                        if v is None and not args.raw_only:
                            v = last.get(spec["dps"])
                        else:
                            last[spec["dps"]] = v
                        row[col] = round(v / spec["scale"], 4) if v is not None else ""
                    writer.writerow(row)
                    fh.flush()
                    if args.echo:
                        print(f"{row['t_s']:>8}s  V={row['voltage_v']}  "
                              f"I={row['current_a']}  P={row['power_w']}"
                              + (f"  E={row.get('energy_kwh')}" if energy else ""))
                if misses >= 10:
                    sys.exit("! 10 consecutive empty responses, giving up")
                sleep = args.interval - (time.monotonic() - loop_start)
                if sleep > 0:
                    time.sleep(sleep)
        except KeyboardInterrupt:
            print(f"\n# stopped, wrote {out}", file=sys.stderr)


def main():
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="cmd", required=True)

    common = argparse.ArgumentParser(add_help=False)
    common.add_argument("--device", default=None,
                        help="device name or id in devices.json (default: first entry)")
    common.add_argument("--interval", type=float, default=1.0,
                        help="seconds between polls (default 1.0)")
    common.add_argument("--duration", type=float, default=0.0,
                        help="stop after N seconds (default: run until Ctrl-C)")

    pr = sub.add_parser("probe", parents=[common], help="dump raw DPS every poll")
    pr.add_argument("--refresh", default=None,
                    help="comma-separated DPS to force-refresh before each read")
    pr.set_defaults(func=cmd_probe)

    lg = sub.add_parser("log", parents=[common], help="write mapped V/I/P CSV")
    lg.add_argument("--out", default="smart-plug-log.csv", help="output CSV path")
    lg.add_argument("--map", default=None,
                    help='JSON overriding the DPS->column map, e.g. '
                         '\'{"voltage_v":{"dps":"20","scale":10},...}\'')
    lg.add_argument("--energy-map", default=None,
                    help='JSON for the cumulative-energy column, or use --no-energy')
    lg.add_argument("--no-energy", action="store_true",
                    help="do not log a cumulative energy column")
    lg.add_argument("--raw-only", action="store_true",
                    help="do not carry values forward across partial-dict polls; "
                         "leave a cell blank when its DPS is absent from that poll")
    lg.add_argument("--refresh", action="store_true",
                    help="force updatedps() before each poll (~5 s/poll on 3.5; "
                         "usually unnecessary, the plug pushes changes itself)")
    lg.add_argument("--echo", action="store_true", help="print each row as it is written")
    lg.set_defaults(func=cmd_log)

    args = p.parse_args()
    if args.device is None:
        # default to the first device in the file
        if DEVICES_JSON.exists():
            entries = json.loads(DEVICES_JSON.read_text())
            if entries:
                args.device = entries[0].get("name") or entries[0].get("id")
    args.func(args)


if __name__ == "__main__":
    main()
