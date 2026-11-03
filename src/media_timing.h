#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <utility>

struct AVFrame;

namespace airscreen {

class AudioTimeline {
public:
    static int64_t duration_ns(size_t samples, int samples_per_second) {
        return (int64_t) samples * 1000000000 / samples_per_second;
    }

    void append(int64_t timestamp_ns, size_t samples) {
        if (samples) queued_.push_back({timestamp_ns, samples, 0});
    }

    void discard(size_t samples, int samples_per_second) {
        while (samples && !queued_.empty()) {
            auto &span = queued_.front();
            size_t n = std::min(samples, span.samples);
            span.timestamp_ns += duration_ns(n, samples_per_second);
            span.samples -= n;
            samples -= n;
            if (!span.samples) queued_.pop_front();
        }
    }

    void schedule(size_t samples, int64_t due_ns, int samples_per_second, int64_t now_ns) {
        prune(now_ns, samples_per_second);
        while (samples && !queued_.empty()) {
            auto span = queued_.front();
            size_t n = std::min(samples, span.samples);
            playing_.push_back({span.timestamp_ns, n, due_ns});
            due_ns += duration_ns(n, samples_per_second);
            discard(n, samples_per_second);
            samples -= n;
        }
    }

    int64_t position(int64_t now_ns, int samples_per_second) {
        prune(now_ns, samples_per_second);
        if (playing_.empty() || now_ns < playing_.front().due_ns || playing_.front().timestamp_ns <= 0) return 0;
        const auto &span = playing_.front();
        return span.timestamp_ns + now_ns - span.due_ns;
    }

    void clear() {
        queued_.clear();
        playing_.clear();
    }

private:
    struct Span {
        int64_t timestamp_ns;
        size_t samples;
        int64_t due_ns;
    };

    void prune(int64_t now_ns, int samples_per_second) {
        while (!playing_.empty() &&
               now_ns >= playing_.front().due_ns + duration_ns(playing_.front().samples, samples_per_second)) {
            playing_.pop_front();
        }
    }

    std::deque<Span> queued_;
    std::deque<Span> playing_;
};

struct VideoFrame {
    std::shared_ptr<AVFrame> data;
    int64_t timestamp_ns = 0;
    uint64_t generation = 0;
};

class VideoFrameQueue {
public:
    static constexpr size_t capacity = 16;

    void push(VideoFrame frame) {
        if (frames_.size() == capacity) {
            frames_.pop_front();
            ++discarded_;
        }
        frames_.push_back(std::move(frame));
    }

    std::optional<VideoFrame> take(int64_t audio_time_ns) {
        if (frames_.empty()) return std::nullopt;
        size_t ready = 0;
        while (ready < frames_.size() &&
               (!audio_time_ns || !frames_[ready].timestamp_ns || frames_[ready].timestamp_ns <= audio_time_ns)) {
            ++ready;
        }
        if (!ready) return std::nullopt;
        discarded_ += ready - 1;
        VideoFrame frame = std::move(frames_[ready - 1]);
        frames_.erase(frames_.begin(), frames_.begin() + ready);
        return frame;
    }

    bool empty() const { return frames_.empty(); }
    size_t size() const { return frames_.size(); }
    uint64_t discarded() const { return discarded_; }
    void clear() { frames_.clear(); }

private:
    std::deque<VideoFrame> frames_;
    uint64_t discarded_ = 0;
};

} // namespace airscreen
