// ScreenCTR — Copyright (C) 2026 Heivoli Network — GPL-3.0-or-later
// FPS, latency and connection quality estimation. No Qt dependency.
#pragma once
#include <cstdint>
#include <deque>

namespace sc {

class FpsCounter {
public:
    void frame(int64_t nowMs) { t_.push_back(nowMs); trim(nowMs); }
    double fps(int64_t nowMs) { trim(nowMs); return (double)t_.size() * 1000.0 / windowMs; }
    static constexpr int64_t windowMs = 1000;
private:
    void trim(int64_t now) { while (!t_.empty() && now - t_.front() >= windowMs) t_.pop_front(); }
    std::deque<int64_t> t_;
};

// Round-trip time from PING/PONG, smoothed like TCP's SRTT (RFC 6298).
class RttEstimator {
public:
    void sample(double rttMs);
    double srtt() const { return srtt_; }
    double jitter() const { return rttvar_; }
    bool valid() const { return n_ > 0; }
private:
    double srtt_ = 0, rttvar_ = 0; int n_ = 0;
};

// Frame loss from frame ids actually completed per screen.
class LossTracker {
public:
    void completed(int screen, uint32_t frameId);
    // Loss ratio (0..1) over the frames seen since the last call; resets.
    double takeLossRatio();
private:
    bool have_[2] = {false, false}; uint32_t last_[2] = {0, 0};
    uint64_t got_ = 0, lost_ = 0;
};

enum class LinkQuality { Unknown, Excellent, Good, Fair, Poor };
// Score 0..100 from loss ratio, RTT and jitter.
int qualityScore(double lossRatio, double rttMs, double jitterMs);
LinkQuality qualityFromScore(int score);
const char *qualityLabel(LinkQuality q);

} // namespace sc
