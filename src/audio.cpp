#include "audio.h"
#include "settings.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <objbase.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <mmreg.h>
#include <cstring>
#include <algorithm>
#include <cstdio>
#include <cstdarg>
#include <avrt.h>

#pragma comment(lib, "avrt.lib")

extern "C" {
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/channel_layout.h>
#include <libavutil/error.h>
}

namespace airscreen {
namespace {

const uint8_t kAacEldExtra[] = {0xF8, 0xE8, 0x50, 0x00};
const uint8_t kAacLcExtra[] = {0x12, 0x10};
const uint8_t kAlacMagic[] = {
    0x00, 0x00, 0x00, 0x24, 0x61, 0x6c, 0x61, 0x63, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x60, 0x00, 0x10, 0x28, 0x0a, 0x0e, 0x02, 0x00, 0xff,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xac, 0x44};

const GUID kSubFormatIeeeFloat = {
    0x00000003, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};

void alog(const char *fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    append_log(buf);
}

bool mix_is_float(const WAVEFORMATEX *fmt) {
    if (!fmt) {
        return false;
    }
    if (fmt->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
        return true;
    }
    if (fmt->wFormatTag == WAVE_FORMAT_EXTENSIBLE && fmt->cbSize >= 22) {
        const auto *ex = reinterpret_cast<const WAVEFORMATEXTENSIBLE *>(fmt);
        return IsEqualGUID(ex->SubFormat, kSubFormatIeeeFloat) != 0;
    }
    return false;
}

void set_extra(AVCodecContext *ctx, const uint8_t *src, int n) {
    av_freep(&ctx->extradata);
    ctx->extradata = (uint8_t *) av_malloc((size_t) n + AV_INPUT_BUFFER_PADDING_SIZE);
    if (!ctx->extradata) {
        ctx->extradata_size = 0;
        return;
    }
    memcpy(ctx->extradata, src, (size_t) n);
    memset(ctx->extradata + n, 0, AV_INPUT_BUFFER_PADDING_SIZE);
    ctx->extradata_size = n;
}

void write_silence(BYTE *dest, UINT32 frames, int channels, int bits) {
    if (!dest || !frames) {
        return;
    }
    int bytes_ps = bits > 0 ? bits / 8 : 2;
    memset(dest, 0, (size_t) frames * (size_t) channels * (size_t) bytes_ps);
}

// Fixed jitter buffer depth. Could be made adaptive if Wi-Fi jitter varies a lot between setups.
const int kTargetMs = 80; // cushion to build before (re)starting playback
const int kMaxMs = 250;   // past this, skip back to kTargetMs

size_t ms_to_samples(int rate, int ch, int ms) {
    return (size_t) rate * (size_t) ch * (size_t) ms / 1000;
}

DWORD WINAPI audio_thread_proc(LPVOID p) {
    auto *self = static_cast<AudioPipeline *>(p);
    self->render_thread();
    return 0;
}

} // namespace

AudioPipeline::AudioPipeline() = default;

AudioPipeline::~AudioPipeline() {
    stop();
}

bool AudioPipeline::start(int sample_rate, int channels, unsigned char codec_type) {
    std::lock_guard<std::mutex> life(life_mu_);
    int rate = sample_rate > 0 ? sample_rate : 44100;
    int ch = channels > 0 ? channels : 2;
    if (running_ && codec_type_ == codec_type && src_ch_ == ch && src_rate_ == rate && wasapi_ok_) {
        return true;
    }
    stop_locked();
    src_rate_ = rate;
    src_ch_ = ch;
    codec_type_ = codec_type;
    if (!init_decoder(codec_type)) {
        alog("audio decoder init failed ct=%u", codec_type);
        return false;
    }
    ready_event_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    wasapi_ok_ = false;
    running_ = true;
    wasapi_thread_ = CreateThread(nullptr, 0, audio_thread_proc, this, 0, nullptr);
    if (!wasapi_thread_) {
        running_ = false;
        alog("audio thread create failed");
        stop_locked();
        return false;
    }
    HANDLE ready = (HANDLE) ready_event_;
    if (WaitForSingleObject(ready, 3000) != WAIT_OBJECT_0 || !wasapi_ok_) {
        alog("WASAPI start failed");
        stop_locked();
        return false;
    }
    alog("audio started ct=%u src %dHz %dch wasapi %dHz %dch %s%d", codec_type, src_rate_, src_ch_,
         out_rate_, out_ch_, out_float_ ? "F" : "S", out_bits_);
    return true;
}

void AudioPipeline::stop() {
    std::lock_guard<std::mutex> life(life_mu_);
    stop_locked();
}

void AudioPipeline::stop_locked() {
    running_ = false;
    if (wasapi_event_) {
        SetEvent((HANDLE) wasapi_event_);
    }
    if (wasapi_thread_) {
        WaitForSingleObject((HANDLE) wasapi_thread_, 2000);
        CloseHandle((HANDLE) wasapi_thread_);
        wasapi_thread_ = nullptr;
    }
    if (ready_event_) {
        CloseHandle((HANDLE) ready_event_);
        ready_event_ = nullptr;
    }
    wasapi_ok_ = false;
    std::lock_guard<std::mutex> dlock(dec_mu_);
    if (swr_) {
        swr_free((SwrContext **) &swr_);
    }
    if (av_pkt_) {
        av_packet_free((AVPacket **) &av_pkt_);
    }
    if (av_frame_) {
        av_frame_free((AVFrame **) &av_frame_);
    }
    if (av_codec_) {
        AVCodecContext *c = (AVCodecContext *) av_codec_;
        avcodec_free_context(&c);
        av_codec_ = nullptr;
    }
    pcm_passthrough_ = false;
    src_fmt_ = -1;
    std::lock_guard<std::mutex> lock(mu_);
    if (rebuffers_ || dropped_) {
        alog("audio stats: %u rebuffers, %u ms dropped", rebuffers_,
             (unsigned) (dropped_ / std::max<size_t>(1, ms_to_samples(out_rate_, out_ch_, 1))));
    }
    rebuffers_ = 0;
    dropped_ = 0;
    buffering_ = true;
    ring_.clear();
    rpos_ = wpos_ = count_ = 0;
}

void AudioPipeline::flush() {
    std::lock_guard<std::mutex> dlock(dec_mu_);
    std::lock_guard<std::mutex> lock(mu_);
    rpos_ = wpos_ = count_ = 0;
    buffering_ = true;
    if (av_codec_) {
        avcodec_flush_buffers((AVCodecContext *) av_codec_);
    }
}

void AudioPipeline::set_volume(float linear) {
    if (linear < 0) {
        linear = 0;
    }
    if (linear > 1) {
        linear = 1;
    }
    volume_ = linear;
}

bool AudioPipeline::init_decoder(unsigned char codec_type) {
    if (codec_type == 0 || codec_type == 1) {
        pcm_passthrough_ = true;
        return true;
    }
    pcm_passthrough_ = false;
    AVCodecID id = AV_CODEC_ID_AAC;
    if (codec_type == 2) {
        id = AV_CODEC_ID_ALAC;
    }
    const AVCodec *codec = avcodec_find_decoder(id);
    if (!codec) {
        return false;
    }
    AVCodecContext *ctx = avcodec_alloc_context3(codec);
    if (!ctx) {
        return false;
    }
    ctx->sample_rate = src_rate_;
    av_channel_layout_default(&ctx->ch_layout, src_ch_);
    ctx->request_sample_fmt = AV_SAMPLE_FMT_S16;
    ctx->flags |= AV_CODEC_FLAG_LOW_DELAY;
    if (id == AV_CODEC_ID_ALAC) {
        set_extra(ctx, kAlacMagic, (int) sizeof(kAlacMagic));
    } else if (codec_type == 4) {
        ctx->profile = AV_PROFILE_AAC_LOW;
        set_extra(ctx, kAacLcExtra, (int) sizeof(kAacLcExtra));
    } else {
        ctx->profile = AV_PROFILE_AAC_ELD;
        set_extra(ctx, kAacEldExtra, (int) sizeof(kAacEldExtra));
    }
    if (!ctx->extradata) {
        avcodec_free_context(&ctx);
        return false;
    }
    int err = avcodec_open2(ctx, codec, nullptr);
    if (err < 0) {
        char es[64];
        av_strerror(err, es, sizeof(es));
        alog("avcodec_open2 failed: %s", es);
        avcodec_free_context(&ctx);
        return false;
    }
    av_codec_ = ctx;
    av_frame_ = av_frame_alloc();
    av_pkt_ = av_packet_alloc();
    return av_frame_ && av_pkt_;
}

bool AudioPipeline::init_wasapi() {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE && hr != S_FALSE) {
        alog("audio CoInitializeEx hr=0x%08lX", (unsigned long) hr);
    }
    IMMDeviceEnumerator *en = nullptr;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&en));
    if (FAILED(hr)) {
        alog("MMDeviceEnumerator hr=0x%08lX", (unsigned long) hr);
        return false;
    }
    IMMDevice *dev = nullptr;
    hr = en->GetDefaultAudioEndpoint(eRender, eConsole, &dev);
    if (FAILED(hr)) {
        hr = en->GetDefaultAudioEndpoint(eRender, eMultimedia, &dev);
    }
    en->Release();
    if (FAILED(hr)) {
        alog("GetDefaultAudioEndpoint hr=0x%08lX", (unsigned long) hr);
        return false;
    }
    IAudioClient *client = nullptr;
    hr = dev->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void **) &client);
    dev->Release();
    if (FAILED(hr) || !client) {
        alog("IAudioClient activate hr=0x%08lX", (unsigned long) hr);
        return false;
    }

    IAudioClient2 *client2 = nullptr;
    if (SUCCEEDED(client->QueryInterface(__uuidof(IAudioClient2), (void **) &client2)) && client2) {
        AudioClientProperties props = {};
        props.cbSize = sizeof(props);
        props.eCategory = AudioCategory_Media;
        client2->SetClientProperties(&props);
        client2->Release();
    }

    WAVEFORMATEX *mix = nullptr;
    hr = client->GetMixFormat(&mix);
    if (FAILED(hr) || !mix) {
        alog("GetMixFormat hr=0x%08lX", (unsigned long) hr);
        client->Release();
        return false;
    }

    REFERENCE_TIME buf100ns = 50 * 10000;
    hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                            AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST,
                            buf100ns, 0, mix, nullptr);
    if (FAILED(hr)) {
        alog("WASAPI Initialize hr=0x%08lX tag=%u %luHz %uch %ubit", (unsigned long) hr,
             mix->wFormatTag, (unsigned long) mix->nSamplesPerSec, mix->nChannels, mix->wBitsPerSample);
        CoTaskMemFree(mix);
        client->Release();
        return false;
    }

    HANDLE ev = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    hr = client->SetEventHandle(ev);
    if (FAILED(hr)) {
        alog("SetEventHandle hr=0x%08lX", (unsigned long) hr);
        CloseHandle(ev);
        CoTaskMemFree(mix);
        client->Release();
        return false;
    }
    UINT32 frames = 0;
    client->GetBufferSize(&frames);
    IAudioRenderClient *render = nullptr;
    hr = client->GetService(__uuidof(IAudioRenderClient), (void **) &render);
    if (FAILED(hr) || !render) {
        alog("IAudioRenderClient hr=0x%08lX", (unsigned long) hr);
        CloseHandle(ev);
        CoTaskMemFree(mix);
        client->Release();
        return false;
    }

    out_rate_ = (int) mix->nSamplesPerSec;
    out_ch_ = (int) mix->nChannels;
    if (out_ch_ <= 0) {
        out_ch_ = 2;
    }
    out_bits_ = (int) mix->wBitsPerSample;
    if (out_bits_ <= 0) {
        out_bits_ = mix_is_float(mix) ? 32 : 16;
    }
    out_float_ = mix_is_float(mix);
    CoTaskMemFree(mix);

    BYTE *pre = nullptr;
    UINT32 preroll = frames / 4;
    if (preroll < 128) {
        preroll = 128;
    }
    if (preroll > frames) {
        preroll = frames;
    }
    if (SUCCEEDED(render->GetBuffer(preroll, &pre)) && pre) {
        write_silence(pre, preroll, out_ch_, out_bits_);
        render->ReleaseBuffer(preroll, 0);
    }
    hr = client->Start();
    if (FAILED(hr)) {
        alog("IAudioClient Start hr=0x%08lX", (unsigned long) hr);
        render->Release();
        CloseHandle(ev);
        client->Release();
        return false;
    }

    wasapi_client_ = client;
    wasapi_render_ = render;
    wasapi_event_ = ev;
    wasapi_frames_ = frames;

    size_t cap = (size_t) out_rate_ * (size_t) out_ch_;
    if (cap < 1024) {
        cap = 1024;
    }
    std::lock_guard<std::mutex> lock(mu_);
    ring_.assign(cap, 0);
    rpos_ = wpos_ = count_ = 0;
    return true;
}

void AudioPipeline::teardown_wasapi() {
    if (wasapi_render_) {
        ((IAudioRenderClient *) wasapi_render_)->Release();
        wasapi_render_ = nullptr;
    }
    if (wasapi_client_) {
        ((IAudioClient *) wasapi_client_)->Stop();
        ((IAudioClient *) wasapi_client_)->Release();
        wasapi_client_ = nullptr;
    }
    if (wasapi_event_) {
        CloseHandle((HANDLE) wasapi_event_);
        wasapi_event_ = nullptr;
    }
    CoUninitialize();
}

bool AudioPipeline::ensure_swr(int in_fmt, int in_rate, int in_ch) {
    if (swr_ && src_fmt_ == in_fmt && swr_in_rate_ == in_rate && swr_in_ch_ == in_ch) {
        return true;
    }
    if (swr_) {
        swr_free((SwrContext **) &swr_);
    }
    AVChannelLayout in_layout;
    AVChannelLayout out_layout;
    av_channel_layout_default(&in_layout, in_ch > 0 ? in_ch : src_ch_);
    av_channel_layout_default(&out_layout, out_ch_);
    SwrContext *swr = nullptr;
    int werr = swr_alloc_set_opts2(&swr, &out_layout, AV_SAMPLE_FMT_S16, out_rate_, &in_layout,
                                   (AVSampleFormat) in_fmt, in_rate, 0, nullptr);
    av_channel_layout_uninit(&in_layout);
    av_channel_layout_uninit(&out_layout);
    if (werr < 0 || !swr || swr_init(swr) < 0) {
        swr_free(&swr);
        alog("swr_init failed fmt=%d %dHz/%dch -> %dHz/%dch", in_fmt, in_rate, in_ch, out_rate_,
             out_ch_);
        return false;
    }
    swr_ = swr;
    src_fmt_ = in_fmt;
    swr_in_rate_ = in_rate;
    swr_in_ch_ = in_ch;
    return true;
}

void AudioPipeline::convert_and_push(const uint8_t **src, int in_samples) {
    if (!swr_ || in_samples <= 0) {
        return;
    }
    int out_samples = swr_get_out_samples((SwrContext *) swr_, in_samples);
    if (out_samples <= 0) {
        return;
    }
    std::vector<int16_t> pcm((size_t) out_samples * (size_t) out_ch_ + 32);
    uint8_t *dst = (uint8_t *) pcm.data();
    int got = swr_convert((SwrContext *) swr_, &dst, out_samples, src, in_samples);
    if (got > 0) {
        push_pcm(pcm.data(), got, out_ch_);
    }
}

void AudioPipeline::push_pcm(const int16_t *pcm, int frames, int channels) {
    std::lock_guard<std::mutex> lock(mu_);
    if (ring_.empty() || frames <= 0 || channels <= 0) {
        return;
    }
    size_t samples = (size_t) frames * (size_t) channels;
    size_t cap = ring_.size();
    size_t max_keep = ms_to_samples(out_rate_, out_ch_, kMaxMs);
    if (max_keep < samples) {
        max_keep = samples;
    }
    if (max_keep > cap) {
        max_keep = cap;
    }
    if (count_ + samples > max_keep) {
        // Skip back to the target depth rather than riding at max latency.
        size_t drop = count_ + samples - ms_to_samples(out_rate_, out_ch_, kTargetMs);
        if (drop > count_) {
            drop = count_;
        }
        rpos_ = (rpos_ + drop) % cap;
        count_ -= drop;
        dropped_ += drop;
    }
    float vol = volume_.load();
    for (size_t i = 0; i < samples; ++i) {
        int v = (int) (pcm[i] * vol);
        if (v > 32767) {
            v = 32767;
        }
        if (v < -32768) {
            v = -32768;
        }
        ring_[wpos_] = (int16_t) v;
        wpos_ = (wpos_ + 1) % cap;
        if (count_ < cap) {
            ++count_;
        } else {
            rpos_ = (rpos_ + 1) % cap;
        }
    }
}

void AudioPipeline::submit(const uint8_t *data, int len, uint64_t ntp_ns) {
    (void) ntp_ns;
    if (!data || len <= 0) {
        return;
    }
    std::lock_guard<std::mutex> dlock(dec_mu_);
    if (pcm_passthrough_) {
        int in_ch = src_ch_ > 0 ? src_ch_ : 2;
        int frames = len / (2 * in_ch);
        if (frames <= 0) {
            return;
        }
        if (!ensure_swr(AV_SAMPLE_FMT_S16, src_rate_, in_ch)) {
            return;
        }
        const uint8_t *src = data;
        convert_and_push(&src, frames);
        return;
    }
    if (!av_codec_ || !av_pkt_ || !av_frame_) {
        return;
    }
    auto *ctx = (AVCodecContext *) av_codec_;
    auto *pkt = (AVPacket *) av_pkt_;
    auto *frame = (AVFrame *) av_frame_;
    if (av_new_packet(pkt, len) < 0) {
        return;
    }
    memcpy(pkt->data, data, (size_t) len);
    int serr = avcodec_send_packet(ctx, pkt);
    av_packet_unref(pkt);
    if (serr < 0 && serr != AVERROR(EAGAIN)) {
        static int decode_errs = 0;
        if (decode_errs < 8) {
            char es[64];
            av_strerror(serr, es, sizeof(es));
            alog("audio decode err ct=%u len=%d: %s", codec_type_, len, es);
            ++decode_errs;
        }
        return;
    }
    while (avcodec_receive_frame(ctx, frame) == 0) {
        int in_rate = frame->sample_rate > 0 ? frame->sample_rate : src_rate_;
        int in_ch = frame->ch_layout.nb_channels > 0 ? frame->ch_layout.nb_channels : src_ch_;
        if (!ensure_swr(frame->format, in_rate, in_ch)) {
            av_frame_unref(frame);
            continue;
        }
        convert_and_push((const uint8_t **) frame->extended_data, frame->nb_samples);
        av_frame_unref(frame);
    }
}

void AudioPipeline::render_thread() {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    DWORD mmcss_idx = 0;
    HANDLE mmcss = AvSetMmThreadCharacteristicsW(L"Pro Audio", &mmcss_idx);
    if (mmcss) {
        AvSetMmThreadPriority(mmcss, AVRT_PRIORITY_HIGH);
    }
    wasapi_ok_ = init_wasapi();
    if (ready_event_) {
        SetEvent((HANDLE) ready_event_);
    }
    if (!wasapi_ok_) {
        running_ = false;
        return;
    }
    auto *client = (IAudioClient *) wasapi_client_;
    auto *render = (IAudioRenderClient *) wasapi_render_;
    HANDLE ev = (HANDLE) wasapi_event_;
    std::vector<int16_t> tmp;
    while (running_) {
        WaitForSingleObject(ev, 50);
        if (!running_) {
            break;
        }
        UINT32 padding = 0;
        if (FAILED(client->GetCurrentPadding(&padding))) {
            continue;
        }
        UINT32 avail = wasapi_frames_ > padding ? wasapi_frames_ - padding : 0;
        if (avail == 0) {
            continue;
        }
        BYTE *dest = nullptr;
        if (FAILED(render->GetBuffer(avail, &dest)) || !dest) {
            continue;
        }
        size_t need = (size_t) avail * (size_t) out_ch_;
        tmp.resize(need);
        {
            std::lock_guard<std::mutex> lock(mu_);
            size_t cap = ring_.size();
            size_t n = 0;
            size_t target = ms_to_samples(out_rate_, out_ch_, kTargetMs);
            if (buffering_ && count_ >= target) {
                // Cushion is ready; start at target depth even if a burst piled up meanwhile.
                size_t drop = count_ - target;
                rpos_ = (rpos_ + drop) % cap;
                count_ -= drop;
                dropped_ += drop;
                buffering_ = false;
            }
            if (!buffering_) {
                n = count_ < need ? count_ : need;
                for (size_t i = 0; i < n; ++i) {
                    tmp[i] = ring_[rpos_];
                    rpos_ = (rpos_ + 1) % cap;
                }
                count_ -= n;
                if (n < need) {
                    // Underrun: go quiet and rebuild the cushion instead of stuttering on every late packet.
                    buffering_ = true;
                    ++rebuffers_;
                }
            }
            if (n < need) {
                memset(tmp.data() + n, 0, (need - n) * sizeof(int16_t));
            }
        }
        if (out_float_) {
            float *out = (float *) dest;
            for (size_t i = 0; i < need; ++i) {
                out[i] = tmp[i] * (1.0f / 32768.0f);
            }
        } else if (out_bits_ >= 24) {
            int32_t *out = (int32_t *) dest;
            for (size_t i = 0; i < need; ++i) {
                out[i] = (int32_t) tmp[i] << 16;
            }
        } else {
            memcpy(dest, tmp.data(), need * sizeof(int16_t));
        }
        render->ReleaseBuffer(avail, 0);
    }
    if (mmcss) {
        AvRevertMmThreadCharacteristics(mmcss);
    }
    teardown_wasapi();
}

} // namespace airscreen
