// ScreenCTR — Copyright (C) 2026 Heivoli Network — GPL-3.0-or-later
#include "StreamStats.h"
#include <algorithm>
#include <cmath>

namespace sc {

void RttEstimator::sample(double r)
{
    if (r < 0) return;
    if (n_++ == 0) { srtt_ = r; rttvar_ = r / 2; return; }
    rttvar_ = 0.75 * rttvar_ + 0.25 * std::fabs(srtt_ - r);
    srtt_ = 0.875 * srtt_ + 0.125 * r;
}

void LossTracker::completed(int s, uint32_t id)
{
    if (s < 0 || s > 1) return;
    if (have_[s]) {
        int32_t d = (int32_t)(id - last_[s]);
        if (d <= 0) return;                 // duplicate / reordered: ignore
        if (d > 1 && d < 1000) lost_ += (uint64_t)(d - 1);
    }
    have_[s] = true; last_[s] = id; got_++;
}

double LossTracker::takeLossRatio()
{
    uint64_t total = got_ + lost_;
    double r = total ? (double)lost_ / (double)total : 0.0;
    got_ = lost_ = 0;
    return r;
}

int qualityScore(double loss, double rtt, double jitter)
{
    double s = 100.0;
    s -= std::min(60.0, loss * 300.0);                    // 10 % loss -> -30
    s -= std::clamp((rtt - 20.0) * 0.4, 0.0, 30.0);        // 95 ms -> -30
    s -= std::clamp((jitter - 5.0) * 0.5, 0.0, 15.0);
    return (int)std::lround(std::clamp(s, 0.0, 100.0));
}

LinkQuality qualityFromScore(int s)
{
    if (s >= 85) return LinkQuality::Excellent;
    if (s >= 65) return LinkQuality::Good;
    if (s >= 40) return LinkQuality::Fair;
    return LinkQuality::Poor;
}

const char *qualityLabel(LinkQuality q)
{
    switch (q) {
    case LinkQuality::Excellent: return "Excellente";
    case LinkQuality::Good: return "Bonne";
    case LinkQuality::Fair: return "Moyenne";
    case LinkQuality::Poor: return "Faible";
    default: return "—";
    }
}

} // namespace sc
