#include "flatbench/stats.h"

#include <algorithm>

namespace flatbench {

LatencySummary SummarizeLatencies(std::vector<uint64_t> latencies_ns) {
    LatencySummary out;
    if (latencies_ns.empty()) {
        return out;
    }

    uint64_t sum = 0;
    for (size_t i = 0; i < latencies_ns.size(); ++i) {
        sum += latencies_ns[i];
    }
    out.avg_ns = static_cast<double>(sum) / static_cast<double>(latencies_ns.size());

    std::sort(latencies_ns.begin(), latencies_ns.end());
    const size_t last = latencies_ns.size() - 1;
    auto pick = [&](double ratio) -> double {
        return static_cast<double>(latencies_ns[static_cast<size_t>(ratio * static_cast<double>(last))]);
    };
    out.p50_ns = pick(0.50);
    out.p95_ns = pick(0.95);
    out.p99_ns = pick(0.99);
    return out;
}

}  // namespace flatbench
