#pragma once

#include <cstdint>
#include <mutex>
#include <vector>
#include <atomic>

namespace airscreen {

class AudioPipeline {
public:
    AudioPipeline();
    ~AudioPipeline();

    bool start(int sample_rate, int channels, unsigned char codec_type);
    void stop();
    void flush();
    void set_volume(float linear); // 0..1
    void submit(const uint8_t *data, int len, uint64_t ntp_ns);
    bool is_running() const { return running_.load(); }
    void render_thread();

private:
    bool init_decoder(unsigned char codec_type);
    bool init_wasapi();
    void teardown_wasapi();
    void stop_locked();
    void push_pcm(const int16_t *pcm, int frames, int channels);
    bool ensure_swr(int in_fmt, int in_rate, int in_ch);
    void convert_and_push(const uint8_t **src, int in_samples);

    void *av_codec_ = nullptr;
    void *av_frame_ = nullptr;
    void *av_pkt_ = nullptr;
    void *swr_ = nullptr;
    int src_rate_ = 44100;
    int src_ch_ = 2;
    int src_fmt_ = -1;
    int swr_in_rate_ = 0;
    int swr_in_ch_ = 0;

    void *wasapi_client_ = nullptr;
    void *wasapi_render_ = nullptr;
    void *wasapi_event_ = nullptr;
    void *wasapi_thread_ = nullptr;
    void *ready_event_ = nullptr;
    unsigned wasapi_frames_ = 0;
    int out_rate_ = 44100;
    int out_ch_ = 2;
    int out_bits_ = 16;
    bool out_float_ = false;
    std::atomic<bool> wasapi_ok_{false};

    std::mutex life_mu_;
    std::mutex mu_;
    std::mutex dec_mu_;
    std::vector<int16_t> ring_;
    size_t rpos_ = 0;
    size_t wpos_ = 0;
    size_t count_ = 0;
    bool buffering_ = true; // refilling cushion before playing; guarded by mu_
    unsigned rebuffers_ = 0;
    size_t dropped_ = 0;
    std::atomic<float> volume_{1.0f};
    std::atomic<bool> running_{false};
    unsigned char codec_type_ = 0;
    bool pcm_passthrough_ = false;
};

} // namespace airscreen
