# OrderBook

Depth-only Binance order book capture and KDB+ analytics.

This project subscribes to Binance depth streams, publishes order book updates into a q tickerplant, stores the `depth` table, and provides q helpers for reconstructing a live or historical book.

## Components

- `main.cpp`: connects to Binance depth websocket streams and publishes depth updates.
- `depth_snapshot_api.py`: fetches an initial REST depth snapshot for bootstrap.
- `q/tick.q`: tickerplant process on port `5010`.
- `q/rdb.q`: realtime database process on port `5011`.
- `q/log2hdb.q`: replays tickerplant logs into an HDB.
- `q/replay.q`: replays a tickerplant log for inspection.
- `q/OrderBook.q`: order book reconstruction helpers.

## Schema

The project stores one table only:

| Column | Type | Description |
| :--- | :--- | :--- |
| `time` | `timestamp` | Exchange event timestamp |
| `sym` | `symbol` | Trading pair, e.g. `BTCUSDT` |
| `received_at` | `timestamp` | Local receive timestamp |
| `side` | `symbol` | `buy` or `sell` |
| `price` | `float` | Order book price level |
| `size` | `float` | Quantity available at the price level |

## Run

Start the tickerplant:

```bash
q q/tick.q
```

Start the RDB:

```bash
q q/rdb.q
```

Build and run the depth publisher:

```bash
make
./binance
```

Convert a tickerplant log into HDB partitions:

```bash
q q/log2hdb.q -logFile log/tplog_$(date +%Y.%m.%d) -hdbDir hdb
```

## Analytics

Load the schema and order book helpers:

```q
\l common/schema.q
```

Use `GetOrderBook` on a filtered depth table:

```q
book:GetOrderBook select from depth where sym=`BTCUSDT
GetBids[book;10]
GetAsks[book;10]
```

## Throughput Notes

### 100-symbol smoke run, 2026-09-23

Command path:

```bash
make
q q/tick.q
q q/rdb.q
timeout 10s ./binance
q q/log2hdb.q -logFile log/tplog_2026.09.23 -hdbDir hdb
```

Configuration:

- `SYMBOL_COUNT = 9`
- `SNAPSHOT_LIMIT = 5000`
- Depth streams only: `<symbol>@depth@100ms`

Observed storage result:

| Store | Rows | Symbols | Time range |
| :--- | ---: | ---: | :--- |
| `intraday/2026.09.23/depth` | 78,953 | 9 | `2026.09.23D14:15:49.429000000` to `2026.09.23D14:15:57.697000000` |
| `hdb/2026.09.23/depth` | 78,953 | 9 | `2026.09.23D14:15:49.429000000` to `2026.09.23D14:15:57.697000000` |

Approximate throughput:

- About 7,895 rows/sec over the 10-second wall-clock run.
- About 9,549 rows/sec over the recorded event-time span.

I
