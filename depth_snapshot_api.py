#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import json
import sys
import time
import urllib.parse
import urllib.request
from pathlib import Path


BINANCE_DEPTH_URL = "https://api.binance.com/api/v3/depth"
FIELDS = (
    "time_ms",
    "sym",
    "received_at_ns",
    "side",
    "price",
    "size",
    "last_update_id",
)


def fetch_snapshot(symbol: str, limit: int) -> dict:
    params = urllib.parse.urlencode({"symbol": symbol.upper(), "limit": limit})
    url = f"{BINANCE_DEPTH_URL}?{params}"

    with urllib.request.urlopen(url, timeout=10) as response:
        if response.status != 200:
            raise RuntimeError(f"Binance returned HTTP {response.status}")
        return json.loads(response.read().decode("utf-8"))


def snapshot_rows(symbol: str, snapshot: dict) -> list[dict]:
    received_at_ns = time.time_ns()
    time_ms = received_at_ns // 1_000_000
    last_update_id = int(snapshot["lastUpdateId"])
    sym = symbol.upper()

    rows: list[dict] = []
    for price, size in snapshot.get("bids", []):
        rows.append(
            {
                "time_ms": time_ms,
                "sym": sym,
                "received_at_ns": received_at_ns,
                "side": "buy",
                "price": price,
                "size": size,
                "last_update_id": last_update_id,
            }
        )

    for price, size in snapshot.get("asks", []):
        rows.append(
            {
                "time_ms": time_ms,
                "sym": sym,
                "received_at_ns": received_at_ns,
                "side": "sell",
                "price": price,
                "size": size,
                "last_update_id": last_update_id,
            }
        )

    return rows


def write_csv(rows: list[dict], include_header: bool, out_path: str | None) -> None:
    out = Path(out_path).open("w", newline="") if out_path else sys.stdout
    writer = csv.DictWriter(out, fieldnames=FIELDS)
    if include_header:
        writer.writeheader()
    writer.writerows(rows)
    if out_path:
        out.close()


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Fetch a Binance full order book snapshot for bootstrap."
    )
    parser.add_argument("--symbol", default="BTCUSDT", help="Binance symbol, e.g. BTCUSDT")
    parser.add_argument(
        "--limit",
        type=int,
        default=5000,
        help="Snapshot depth limit. Binance allows 1-5000 and truncates above 5000.",
    )
    parser.add_argument(
        "--no-header",
        action="store_true",
        help="Do not write the CSV header. Used by q loader.",
    )
    parser.add_argument(
        "--out",
        help="Optional CSV output path. Defaults to stdout.",
    )
    args = parser.parse_args()

    if not 1 <= args.limit <= 5000:
        parser.error("--limit must be between 1 and 5000")

    snapshot = fetch_snapshot(args.symbol, args.limit)
    rows = snapshot_rows(args.symbol, snapshot)
    write_csv(rows, include_header=not args.no_header, out_path=args.out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
