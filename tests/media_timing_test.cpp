#include "media_timing.h"

#include <cstdio>
#include <cstdlib>

using namespace airscreen;

void require(bool condition, const char *message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

int main() {
    constexpr int rate = 96000;
    constexpr int64_t ms = 1000000;
    AudioTimeline audio;
    audio.append(1000 * ms, 960);
    audio.append(1300 * ms, 960);
    audio.schedule(1920, 100 * ms, rate, 80 * ms);
    require(audio.position(99 * ms, rate) == 0, "audio must not start before endpoint padding drains");
    require(audio.position(105 * ms, rate) == 1005 * ms, "first audio packet follows the output clock");
    require(audio.position(115 * ms, rate) == 1305 * ms, "a packet timestamp gap must survive playback");
    require(audio.position(120 * ms, rate) == 0, "silence must not advance an expired media clock");

    audio.clear();
    audio.append(1000 * ms, 9600);
    audio.discard(7680, rate);
    audio.schedule(1920, 100 * ms, rate, 100 * ms);
    require(audio.position(105 * ms, rate) == 1085 * ms, "audio catch-up must advance its media timestamp");
    audio.clear();
    require(audio.position(105 * ms, rate) == 0, "flush must invalidate the old audio clock");

    VideoFrameQueue video;
    video.push({{}, 100 * ms, 1});
    video.push({{}, 116 * ms, 1});
    video.push({{}, 133 * ms, 1});
    require(!video.take(99 * ms), "video must wait for audio without consuming future frames");
    auto frame = video.take(120 * ms);
    require(frame && frame->timestamp_ns == 116 * ms, "select the newest due decoded frame");
    require(video.size() == 1 && video.discarded() == 1, "discard stale display frames but retain the future frame");
    frame = video.take(0);
    require(frame && frame->timestamp_ns == 133 * ms, "a silent stream must display its final frame immediately");
    video.clear();
    require(video.empty(), "session reset must remove pending display frames");

    VideoFrameQueue overloaded;
    uint64_t rendered = 0;
    int64_t last_lag = 0;
    for (int i = 1; i <= 36000; ++i) {
        int64_t now = (int64_t) i * 1000000000 / 60;
        overloaded.push({{}, now, 2});
        require(overloaded.size() <= VideoFrameQueue::capacity, "display storage must remain bounded");
        if (i % 15 == 0) {
            auto latest = overloaded.take(now - 80 * ms);
            require(latest.has_value(), "slow display must still produce synchronized frames");
            last_lag = now - 80 * ms - latest->timestamp_ns;
            require(last_lag >= 0 && last_lag < 17 * ms, "display lag must not accumulate during ten minutes of overload");
            ++rendered;
        }
    }
    require(overloaded.discarded() > 30000, "overload must replace decoded frames rather than build a backlog");
    std::printf("PASS: audio timestamps, queue catch-up, silence, flush, and 600s at 60 fps / 4 fps display; rendered=%llu max_lag<17ms\n",
                (unsigned long long) rendered);
    return 0;
}
