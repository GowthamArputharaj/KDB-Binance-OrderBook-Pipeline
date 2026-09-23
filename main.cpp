#include "include/websocket.hpp"
#include <array>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "include/simdjson.h"
#include "include/parser.hpp"
#include "include/tickerplant.hpp"
#include "include/symbols.hpp"

namespace {
constexpr const char* WS_URL = "stream.binance.com";
constexpr const char* WS_PORT = "9443";
constexpr size_t SYMBOL_COUNT = 100;
constexpr int SNAPSHOT_LIMIT = 5000;

struct SnapshotDepthRow {
    long long time_ms;
    std::string symbol;
    long long received_at_ns;
    std::string side;
    double price;
    double size;
    long long last_update_id;
};

std::vector<std::string> get_market_streams(size_t symbol_count) {
    return get_depth_symbols(symbol_count);
}

std::vector<std::string> split_csv_line(const std::string& line) {
    std::vector<std::string> fields;
    std::stringstream ss(line);
    std::string field;
    while (std::getline(ss, field, ',')) {
        fields.push_back(field);
    }
    return fields;
}

bool parse_snapshot_row(const std::string& line, SnapshotDepthRow& row) {
    auto fields = split_csv_line(line);
    if (fields.size() != 7 || fields[0] == "time_ms") {
        return false;
    }

    row.time_ms = std::stoll(fields[0]);
    row.symbol = fields[1];
    row.received_at_ns = std::stoll(fields[2]);
    row.side = fields[3];
    row.price = std::stod(fields[4]);
    row.size = std::stod(fields[5]);
    row.last_update_id = std::stoll(fields[6]);
    return true;
}

size_t publish_depth_snapshot(TickerPlant& ticker_plant, const std::string& symbol) {
    std::string command = "python3 depth_snapshot_api.py --symbol " + symbol +
                          " --limit " + std::to_string(SNAPSHOT_LIMIT);

    std::array<char, 4096> buffer{};
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        std::cerr << "[snapshot] Failed to run: " << command << "\n";
        return 0;
    }

    std::string output;
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
    }

    int exit_code = pclose(pipe);
    if (exit_code != 0) {
        std::cerr << "[snapshot] Python fetch failed for " << symbol
                  << " exit=" << exit_code << "\n";
        return 0;
    }

    std::stringstream lines(output);
    std::string line;
    size_t published_count = 0;
    long long last_update_id = 0;
    while (std::getline(lines, line)) {
        SnapshotDepthRow row;
        if (!parse_snapshot_row(line, row)) {
            continue;
        }

        if (ticker_plant.publish_depth(
                "depth",
                row.time_ms,
                row.symbol,
                row.received_at_ns,
                row.side,
                row.price,
                row.size)) {
            ++published_count;
            last_update_id = row.last_update_id;
        }
    }

    std::cout << "[snapshot] " << symbol
              << " rows=" << published_count
              << " lastUpdateId=" << last_update_id
              << "\n";
    return published_count;
}
}

int main() {
    TickerPlant ticker_plant;
    if (!ticker_plant.connect("127.0.0.1", 5010)) {
        return 1;
    }
    std::cout << "TickerPlant initialized. Awaiting market feed...\n";

    auto base_symbols = get_base_symbols(SYMBOL_COUNT);
    for (const auto& symbol : base_symbols) {
        publish_depth_snapshot(ticker_plant, symbol);
    }

    asio::io_context io;
    asio::ssl::context ssl(asio::ssl::context::tls_client);
    ssl.set_default_verify_paths();

    WebSocket socket(io, ssl, WS_URL, WS_PORT);
    socket.connect();

    auto streams = get_market_streams(SYMBOL_COUNT);
    socket.subscribe(streams);

    std::cout << "Subscribed to " << streams.size() << " streams from "
              << SYMBOL_COUNT << " symbols\n";
    for (const auto& stream : streams) {
        std::cout << "  " << stream << "\n";
    }

    while (true) {
        auto msg = socket.read();

        if (msg.find("depthUpdate") != std::string::npos) {
            std::cout << msg;
            auto updates = parse_depth(msg);
            size_t published_count = 0;
            for (const auto& update : updates) {
                if (ticker_plant.publish_depth(
                    "depth",
                    update.event_time,
                    update.symbol,
                    update.received_at,
                    update.side,
                    update.price,
                    update.quantity)) {
                    ++published_count;
                }
            }
            if (!updates.empty()) {
                const auto& first = updates.front();
                std::cout << "[depth] "
                          << first.symbol
                          << " rows=" << updates.size()
                          << " published=" << published_count
                          << " first=" << first.side
                          << " " << first.price
                          << " x " << first.quantity
                          << "\n";
            }
        }
    }

    return 0;
}
