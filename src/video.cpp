#include "video.h"
#include "settings.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <d3d11_4.h>
#include <dxgi.h>
#include <dxgi1_3.h>
#include <dxgi1_5.h>
#include <d3dcompiler.h>
#include <avrt.h>
#include <wrl/client.h>
#include <vector>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <chrono>
#include <algorithm>

#pragma comment(lib, "avrt.lib")

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_d3d11va.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/error.h>
}

using Microsoft::WRL::ComPtr;

namespace airscreen {
namespace {

const char *kVs = R"(
struct VSOut { float4 pos : SV_POSITION; float2 uv : TEXCOORD; };
cbuffer CB : register(b0) { float4 letterbox; };
VSOut main(float2 pos : POSITION, float2 uv : TEXCOORD) {
    VSOut o;
    o.pos = float4(pos.x * letterbox.x, pos.y * letterbox.y, 0, 1);
    o.uv = uv;
    return o;
}
)";

const char *kPs = R"(
Texture2D texY : register(t0);
Texture2D texUV : register(t1);
SamplerState samp : register(s0);
cbuffer ColorCB : register(b1) { float full_range; float3 pad; };
float4 main(float4 pos : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET {
    float y = texY.Sample(samp, uv).r;
    float2 chroma = texUV.Sample(samp, uv).rg;
    float d = chroma.r - 0.5;
    float e = chroma.g - 0.5;
    float r, g, b;
    if (full_range > 0.5) {
        r = y + 1.5748 * e;
        g = y - 0.1873 * d - 0.4681 * e;
        b = y + 1.8556 * d;
    } else {
        float c = y - 0.0627451;
        r = 1.164383 * c + 1.792741 * e;
        g = 1.164383 * c - 0.213249 * d - 0.532909 * e;
        b = 1.164383 * c + 2.112402 * d;
    }
    return float4(saturate(r), saturate(g), saturate(b), 1);
}
)";

struct Vertex {
    float x, y, u, v;
};

AVPixelFormat hw_get_format(AVCodecContext *ctx, const AVPixelFormat *fmt) {
    (void) ctx;
    for (int i = 0; fmt[i] != AV_PIX_FMT_NONE; ++i) {
        if (fmt[i] == AV_PIX_FMT_D3D11) {
            return AV_PIX_FMT_D3D11;
        }
    }
    for (int i = 0; fmt[i] != AV_PIX_FMT_NONE; ++i) {
        if (fmt[i] == AV_PIX_FMT_NV12 || fmt[i] == AV_PIX_FMT_YUV420P) {
            return fmt[i];
        }
    }
    return AV_PIX_FMT_NONE;
}

} // namespace

VideoPipeline::VideoPipeline() = default;

VideoPipeline::~VideoPipeline() {
    shutdown();
}

bool VideoPipeline::init(HWND hwnd) {
    stop_worker();
    {
        std::lock_guard<std::mutex> lock(mu_);
        hwnd_ = hwnd;
        if (!create_device()) {
            return false;
        }
        if (!create_swapchain()) {
            return false;
        }
        if (!create_pipeline()) {
            return false;
        }
    }
    start_worker();
    return true;
}

void VideoPipeline::stop_worker() {
    worker_run_ = false;
    q_cv_.notify_all();
    frame_cv_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
    if (renderer_.joinable()) {
        renderer_.join();
    }
    std::lock_guard<std::mutex> qlock(q_mu_);
    q_.clear();
    std::lock_guard<std::mutex> flock(frame_mu_);
    frames_.clear();
}

void VideoPipeline::start_worker() {
    if (worker_run_ || worker_.joinable()) {
        return;
    }
    worker_run_ = true;
    worker_ = std::thread([this] { decode_loop(); });
    renderer_ = std::thread([this] { render_loop(); });
}

void VideoPipeline::shutdown() {
    stop_worker();
    std::lock_guard<std::mutex> lock(mu_);
    decode_close();
    if (staging_) {
        staging_->Release();
        staging_ = nullptr;
        staging_w_ = staging_h_ = 0;
    }
    if (srv_y_) {
        srv_y_->Release();
        srv_y_ = nullptr;
    }
    if (srv_uv_) {
        srv_uv_->Release();
        srv_uv_ = nullptr;
    }
    if (nv12_) {
        nv12_->Release();
        nv12_ = nullptr;
    }
    if (cbuf_) {
        cbuf_->Release();
        cbuf_ = nullptr;
    }
    if (color_cbuf_) {
        color_cbuf_->Release();
        color_cbuf_ = nullptr;
    }
    if (raster_) {
        raster_->Release();
        raster_ = nullptr;
    }
    if (sampler_) {
        sampler_->Release();
        sampler_ = nullptr;
    }
    if (vb_) {
        vb_->Release();
        vb_ = nullptr;
    }
    if (layout_) {
        layout_->Release();
        layout_ = nullptr;
    }
    if (vs_) {
        vs_->Release();
        vs_ = nullptr;
    }
    if (ps_) {
        ps_->Release();
        ps_ = nullptr;
    }
    if (rtv_) {
        rtv_->Release();
        rtv_ = nullptr;
    }
    if (swapchain_) {
        swapchain_->Release();
        swapchain_ = nullptr;
    }
    if (ctx_) {
        ctx_->Release();
        ctx_ = nullptr;
    }
    if (device_) {
        device_->Release();
        device_ = nullptr;
    }
    hwnd_ = nullptr;
}

void VideoPipeline::resize() {
    std::lock_guard<std::mutex> lock(mu_);
    if (!swapchain_ || !ctx_) {
        return;
    }
    ctx_->OMSetRenderTargets(0, nullptr, nullptr);
    if (rtv_) {
        rtv_->Release();
        rtv_ = nullptr;
    }
    UINT flags = tearing_ ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
    HRESULT hr = swapchain_->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, flags);
    if (FAILED(hr)) {
        return;
    }
    present_cw_ = present_ch_ = 0;
    ID3D11Texture2D *back = nullptr;
    if (SUCCEEDED(swapchain_->GetBuffer(0, IID_PPV_ARGS(&back)))) {
        device_->CreateRenderTargetView(back, nullptr, &rtv_);
        back->Release();
    }
}

void VideoPipeline::flush() {
    std::scoped_lock lock(decode_mu_, mu_);
    ++generation_;
    {
        std::lock_guard<std::mutex> qlock(q_mu_);
        q_.clear();
    }
    {
        std::lock_guard<std::mutex> flock(frame_mu_);
        frames_.clear();
    }
    if (av_codec_) {
        avcodec_flush_buffers((AVCodecContext *) av_codec_);
    }
    q_cv_.notify_all();
    frame_cv_.notify_all();
}

void VideoPipeline::clear() {
    flush();
    std::lock_guard<std::mutex> lock(mu_);
    has_frame_ = false;
    if (rtv_ && ctx_ && swapchain_) {
        float black[4] = {0.07f, 0.08f, 0.10f, 1};
        ctx_->OMSetRenderTargets(1, &rtv_, nullptr);
        ctx_->ClearRenderTargetView(rtv_, black);
        swapchain_->Present(0, DXGI_PRESENT_DO_NOT_WAIT | (tearing_ ? DXGI_PRESENT_ALLOW_TEARING : 0));
    }
}

void VideoPipeline::set_playback_clock(std::function<int64_t()> fn) {
    std::lock_guard<std::mutex> lock(mu_);
    playback_clock_ = std::move(fn);
}

void VideoPipeline::set_cover(bool cover) {
    cover_.store(cover);
}

void VideoPipeline::set_first_frame_fn(std::function<void()> fn) {
    std::lock_guard<std::mutex> lock(mu_);
    first_frame_fn_ = std::move(fn);
}

void VideoPipeline::set_source_size(int width, int height) {
    width_.store(width);
    height_.store(height);
}

bool VideoPipeline::create_device() {
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_VIDEO_SUPPORT;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    D3D_FEATURE_LEVEL got = D3D_FEATURE_LEVEL_11_0;
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, levels, 3,
                                   D3D11_SDK_VERSION, &device_, &got, &ctx_);
    if (FAILED(hr)) {
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_VIDEO_SUPPORT,
                               levels, 3, D3D11_SDK_VERSION, &device_, &got, &ctx_);
    }
    if (FAILED(hr)) {
        return false;
    }
    ComPtr<ID3D11Multithread> mt;
    if (SUCCEEDED(ctx_->QueryInterface(IID_PPV_ARGS(&mt)))) {
        mt->SetMultithreadProtected(TRUE);
    }
    return true;
}

bool VideoPipeline::create_swapchain() {
    ComPtr<IDXGIDevice> dxgi_dev;
    device_->QueryInterface(IID_PPV_ARGS(&dxgi_dev));
    ComPtr<IDXGIAdapter> adapter;
    dxgi_dev->GetAdapter(&adapter);
    ComPtr<IDXGIFactory2> factory;
    adapter->GetParent(IID_PPV_ARGS(&factory));

    BOOL allow_tearing = FALSE;
    ComPtr<IDXGIFactory5> factory5;
    if (SUCCEEDED(factory.As(&factory5))) {
        factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow_tearing,
                                     sizeof(allow_tearing));
    }
    tearing_ = allow_tearing != FALSE;

    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.Flags = tearing_ ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
    desc.Scaling = DXGI_SCALING_STRETCH;

    HRESULT hr = factory->CreateSwapChainForHwnd(device_, hwnd_, &desc, nullptr, nullptr, &swapchain_);
    if (FAILED(hr)) {
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        hr = factory->CreateSwapChainForHwnd(device_, hwnd_, &desc, nullptr, nullptr, &swapchain_);
        if (FAILED(hr) && tearing_) {
            tearing_ = false;
            desc.Flags = 0;
            hr = factory->CreateSwapChainForHwnd(device_, hwnd_, &desc, nullptr, nullptr, &swapchain_);
        }
        if (FAILED(hr)) {
            tearing_ = false;
            return false;
        }
    }
    factory->MakeWindowAssociation(hwnd_, DXGI_MWA_NO_ALT_ENTER);

    ComPtr<IDXGIDevice1> dxgi_dev1;
    if (SUCCEEDED(dxgi_dev.As(&dxgi_dev1))) {
        hr = dxgi_dev1->SetMaximumFrameLatency(1);
        if (FAILED(hr)) {
            return false;
        }
    }

    ID3D11Texture2D *back = nullptr;
    swapchain_->GetBuffer(0, IID_PPV_ARGS(&back));
    device_->CreateRenderTargetView(back, nullptr, &rtv_);
    back->Release();
    return true;
}

bool VideoPipeline::create_pipeline() {
    ComPtr<ID3DBlob> vsb, psb, err;
    HRESULT hr = D3DCompile(kVs, strlen(kVs), "vs", nullptr, nullptr, "main", "vs_4_0", 0, 0, &vsb, &err);
    if (FAILED(hr)) {
        return false;
    }
    hr = D3DCompile(kPs, strlen(kPs), "ps", nullptr, nullptr, "main", "ps_4_0", 0, 0, &psb, &err);
    if (FAILED(hr)) {
        return false;
    }
    device_->CreateVertexShader(vsb->GetBufferPointer(), vsb->GetBufferSize(), nullptr, &vs_);
    device_->CreatePixelShader(psb->GetBufferPointer(), psb->GetBufferSize(), nullptr, &ps_);

    D3D11_INPUT_ELEMENT_DESC ied[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    device_->CreateInputLayout(ied, 2, vsb->GetBufferPointer(), vsb->GetBufferSize(), &layout_);

    Vertex quad[] = {
        {-1, 1, 0, 0}, {1, 1, 1, 0}, {-1, -1, 0, 1}, {1, -1, 1, 1},
    };
    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = sizeof(quad);
    bd.Usage = D3D11_USAGE_IMMUTABLE;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA srd = {quad};
    device_->CreateBuffer(&bd, &srd, &vb_);

    D3D11_BUFFER_DESC cbd = {};
    cbd.ByteWidth = 16;
    cbd.Usage = D3D11_USAGE_DYNAMIC;
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    device_->CreateBuffer(&cbd, nullptr, &cbuf_);

    D3D11_BUFFER_DESC colord = {};
    colord.ByteWidth = 16;
    colord.Usage = D3D11_USAGE_DYNAMIC;
    colord.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    colord.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    device_->CreateBuffer(&colord, nullptr, &color_cbuf_);
    color_cbuf_dirty_ = true;

    D3D11_SAMPLER_DESC sd = {};
    sd.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    device_->CreateSamplerState(&sd, &sampler_);

    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
    device_->CreateRasterizerState(&rd, &raster_);
    return true;
}

bool VideoPipeline::ensure_nv12(int w, int h) {
    if (nv12_ && tex_w_ == w && tex_h_ == h) {
        return true;
    }
    if (srv_y_) {
        srv_y_->Release();
        srv_y_ = nullptr;
    }
    if (srv_uv_) {
        srv_uv_->Release();
        srv_uv_ = nullptr;
    }
    if (nv12_) {
        nv12_->Release();
        nv12_ = nullptr;
    }
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = w;
    td.Height = h;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_NV12;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(device_->CreateTexture2D(&td, nullptr, &nv12_))) {
        return false;
    }
    D3D11_SHADER_RESOURCE_VIEW_DESC svd = {};
    svd.Format = DXGI_FORMAT_R8_UNORM;
    svd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    svd.Texture2D.MipLevels = 1;
    device_->CreateShaderResourceView(nv12_, &svd, &srv_y_);
    svd.Format = DXGI_FORMAT_R8G8_UNORM;
    device_->CreateShaderResourceView(nv12_, &svd, &srv_uv_);
    tex_w_ = w;
    tex_h_ = h;
    return srv_y_ && srv_uv_;
}

bool VideoPipeline::ensure_staging(int w, int h) {
    if (staging_ && staging_w_ == w && staging_h_ == h) {
        return true;
    }
    if (staging_) {
        staging_->Release();
        staging_ = nullptr;
    }
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = w;
    td.Height = h;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_NV12;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_STAGING;
    td.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(device_->CreateTexture2D(&td, nullptr, &staging_))) {
        staging_w_ = staging_h_ = 0;
        return false;
    }
    staging_w_ = w;
    staging_h_ = h;
    return true;
}

bool VideoPipeline::decode_init(bool hevc) {
    decode_close();
    hevc_ = hevc;
    const AVCodec *codec = avcodec_find_decoder(hevc ? AV_CODEC_ID_HEVC : AV_CODEC_ID_H264);
    if (!codec) {
        return false;
    }
    AVCodecContext *ctx = avcodec_alloc_context3(codec);
    ctx->flags |= AV_CODEC_FLAG_LOW_DELAY;
    ctx->flags2 |= AV_CODEC_FLAG2_FAST | AV_CODEC_FLAG2_SHOW_ALL;
    ctx->thread_count = 0;
    ctx->thread_type = FF_THREAD_SLICE;
    ctx->pkt_timebase = AVRational{1, 1000000000};
    ctx->extra_hw_frames = (int) VideoFrameQueue::capacity + 2;
    decoder_reported_ = false;

    AVBufferRef *hw = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D11VA);
    if (hw) {
        auto *hwctx = (AVHWDeviceContext *) hw->data;
        auto *d3 = (AVD3D11VADeviceContext *) hwctx->hwctx;
        d3->device = device_;
        device_->AddRef();
        if (av_hwdevice_ctx_init(hw) == 0) {
            ctx->hw_device_ctx = av_buffer_ref(hw);
            ctx->get_format = hw_get_format;
            av_hw_device_ = hw;
        } else {
            av_buffer_unref(&hw);
        }
    }

    if (avcodec_open2(ctx, codec, nullptr) < 0) {
        avcodec_free_context(&ctx);
        if (av_hw_device_) {
            av_buffer_unref((AVBufferRef **) &av_hw_device_);
        }
        return false;
    }
    av_codec_ = ctx;
    av_frame_ = av_frame_alloc();
    av_pkt_ = av_packet_alloc();
    return true;
}

void VideoPipeline::decode_close() {
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
    if (av_hw_device_) {
        av_buffer_unref((AVBufferRef **) &av_hw_device_);
        av_hw_device_ = nullptr;
    }
}

void VideoPipeline::submit(const uint8_t *data, int len, uint64_t ntp_ns, bool hevc) {
    if (!data || len <= 0 || !worker_run_) {
        return;
    }
    ++received_count_;
    received_bytes_ += (uint64_t) len;
    EncodedPacket pkt;
    pkt.data.assign(data, data + len);
    pkt.ntp_ns = ntp_ns;
    pkt.generation = generation_.load();
    pkt.hevc = hevc;
    auto wait_start = std::chrono::steady_clock::now();
    {
        std::unique_lock<std::mutex> qlock(q_mu_);
        q_cv_.wait(qlock, [this, &pkt] { return q_.size() < kMaxQueued || !worker_run_ || pkt.generation != generation_.load(); });
        if (!worker_run_ || pkt.generation != generation_.load()) {
            return;
        }
        q_.push_back(std::move(pkt));
    }
    uint64_t wait_us = (uint64_t) std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - wait_start).count();
    uint64_t previous = max_submit_wait_us_.load();
    while (previous < wait_us && !max_submit_wait_us_.compare_exchange_weak(previous, wait_us)) {}
    q_cv_.notify_all();
}

void VideoPipeline::decode_loop() {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    DWORD mmcss_idx = 0;
    HANDLE mmcss = AvSetMmThreadCharacteristicsW(L"Games", &mmcss_idx);
    if (mmcss) {
        AvSetMmThreadPriority(mmcss, AVRT_PRIORITY_HIGH);
    }
    while (worker_run_) {
        EncodedPacket pkt;
        {
            std::unique_lock<std::mutex> qlock(q_mu_);
            q_cv_.wait(qlock, [this] { return !q_.empty() || !worker_run_; });
            if (!worker_run_) {
                break;
            }
            pkt = std::move(q_.front());
            q_.pop_front();
        }
        q_cv_.notify_all();
        auto decode_start = std::chrono::steady_clock::now();
        decode_packet(pkt);
        uint64_t decode_us = (uint64_t) std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - decode_start).count();
        uint64_t previous = max_decode_us_.load();
        while (previous < decode_us && !max_decode_us_.compare_exchange_weak(previous, decode_us)) {}
    }
    if (mmcss) {
        AvRevertMmThreadCharacteristics(mmcss);
    }
}

void VideoPipeline::decode_packet(const EncodedPacket &pkt) {
    std::lock_guard<std::mutex> lock(decode_mu_);
    if (!device_ || pkt.data.empty() || pkt.generation != generation_.load()) {
        return;
    }
    if (!av_codec_ || hevc_ != pkt.hevc) {
        if (!decode_init(pkt.hevc)) return;
    }
    auto *ctx = (AVCodecContext *) av_codec_;
    auto *avpkt = (AVPacket *) av_pkt_;
    av_packet_unref(avpkt);
    if (av_new_packet(avpkt, (int) pkt.data.size()) < 0) return;
    memcpy(avpkt->data, pkt.data.data(), pkt.data.size());
    avpkt->pts = pkt.ntp_ns ? (int64_t) pkt.ntp_ns : AV_NOPTS_VALUE;
    int send = avcodec_send_packet(ctx, avpkt);
    if (send == AVERROR(EAGAIN)) {
        receive_frames(pkt.generation);
        send = avcodec_send_packet(ctx, avpkt);
    }
    av_packet_unref(avpkt);
    if (send < 0 && send != AVERROR(EAGAIN)) {
        char error[64];
        av_strerror(send, error, sizeof(error));
        char line[128];
        snprintf(line, sizeof(line), "video decode send failed: %s", error);
        append_log(line);
        if (decode_init(pkt.hevc)) {
            ctx = (AVCodecContext *) av_codec_;
            avpkt = (AVPacket *) av_pkt_;
            if (av_new_packet(avpkt, (int) pkt.data.size()) >= 0) {
                memcpy(avpkt->data, pkt.data.data(), pkt.data.size());
                avpkt->pts = pkt.ntp_ns ? (int64_t) pkt.ntp_ns : AV_NOPTS_VALUE;
                send = avcodec_send_packet(ctx, avpkt);
                av_packet_unref(avpkt);
                if (send >= 0) receive_frames(pkt.generation);
            }
        }
        return;
    }
    receive_frames(pkt.generation);
}

void VideoPipeline::receive_frames(uint64_t generation) {
    auto *ctx = (AVCodecContext *) av_codec_;
    auto *frame = (AVFrame *) av_frame_;
    int result;
    while ((result = avcodec_receive_frame(ctx, frame)) == 0) {
        ++decoded_count_;
        if (!decoder_reported_) {
            char line[128];
            snprintf(line, sizeof(line), "video decoder: %s %dx%d threads=%d",
                     av_get_pix_fmt_name((AVPixelFormat) frame->format), frame->width, frame->height, ctx->thread_count);
            append_log(line);
            decoder_reported_ = true;
        }
        AVFrame *copy = av_frame_clone(frame);
        if (copy) {
            VideoFrame ready;
            ready.data = std::shared_ptr<AVFrame>(copy, [](AVFrame *p) { av_frame_free(&p); });
            ready.timestamp_ns = frame->pts == AV_NOPTS_VALUE ? 0 : frame->pts;
            ready.generation = generation;
            std::lock_guard<std::mutex> lock(frame_mu_);
            if (generation == generation_.load()) frames_.push(std::move(ready));
        }
        av_frame_unref(frame);
        frame_cv_.notify_one();
    }
    if (result != AVERROR(EAGAIN) && result != AVERROR_EOF) {
        char error[64];
        av_strerror(result, error, sizeof(error));
        char line[128];
        snprintf(line, sizeof(line), "video decode receive failed: %s", error);
        append_log(line);
    }
}

bool VideoPipeline::upload_frame(AVFrame *frame) {
    int w = frame->width & ~1;
    int h = frame->height & ~1;
    if (w <= 0 || h <= 0) {
        return false;
    }
    width_.store(w);
    height_.store(h);
    if (frame->format == AV_PIX_FMT_YUVJ420P || frame->color_range == AVCOL_RANGE_JPEG) {
        if (!full_range_) {
            full_range_ = true;
            color_cbuf_dirty_ = true;
        }
    } else if (frame->color_range == AVCOL_RANGE_MPEG) {
        if (full_range_) {
            full_range_ = false;
            color_cbuf_dirty_ = true;
        }
    }
    if (!ensure_nv12(w, h)) {
        return false;
    }
    if (frame->format == AV_PIX_FMT_D3D11) {
        auto *tex = (ID3D11Texture2D *) frame->data[0];
        intptr_t index = (intptr_t) frame->data[1];
        if (!tex) {
            return false;
        }
        D3D11_BOX box = {0, 0, 0, (UINT) w, (UINT) h, 1};
        ctx_->CopySubresourceRegion(nv12_, 0, 0, 0, 0, tex, (UINT) index, &box);
    } else if (frame->format == AV_PIX_FMT_NV12) {
        if (ensure_staging(w, h)) {
            D3D11_MAPPED_SUBRESOURCE map = {};
            if (SUCCEEDED(ctx_->Map(staging_, 0, D3D11_MAP_WRITE, 0, &map))) {
                for (int y = 0; y < h; ++y) {
                    memcpy((uint8_t *) map.pData + y * map.RowPitch, frame->data[0] + y * frame->linesize[0], w);
                }
                uint8_t *dst_uv = (uint8_t *) map.pData + map.RowPitch * h;
                for (int y = 0; y < h / 2; ++y) {
                    memcpy(dst_uv + y * map.RowPitch, frame->data[1] + y * frame->linesize[1], w);
                }
                ctx_->Unmap(staging_, 0);
                ctx_->CopyResource(nv12_, staging_);
            }
        }
    } else if (frame->format == AV_PIX_FMT_YUV420P || frame->format == AV_PIX_FMT_YUVJ420P) {
        upload_yuv420(frame->data[0], frame->linesize[0], frame->data[1], frame->data[2], frame->linesize[1], w, h);
    } else {
        return false;
    }
    return true;
}

void VideoPipeline::render_loop() {
    DWORD mmcss_idx = 0;
    HANDLE mmcss = AvSetMmThreadCharacteristicsW(L"Playback", &mmcss_idx);
    std::optional<VideoFrame> current;
    bool uploaded = false;
    auto last_stats = std::chrono::steady_clock::now();
    uint64_t last_received = received_count_.load();
    uint64_t last_bytes = received_bytes_.load();
    uint64_t last_decoded = decoded_count_.load();
    while (worker_run_) {
        {
            std::unique_lock<std::mutex> lock(frame_mu_);
            if (!current && frames_.empty()) {
                frame_cv_.wait(lock, [this] { return !worker_run_ || !frames_.empty(); });
            }
        }
        if (!worker_run_) break;
        int64_t audio_time;
        {
            std::lock_guard<std::mutex> lock(mu_);
            audio_time = playback_clock_ ? playback_clock_() : 0;
        }
        {
            std::unique_lock<std::mutex> lock(frame_mu_);
            auto next = frames_.take(audio_time);
            if (next) {
                current = std::move(next);
                uploaded = false;
            }
            if (!current) {
                frame_cv_.wait_for(lock, std::chrono::milliseconds(2));
                continue;
            }
        }
        HRESULT hr = E_FAIL;
        {
            std::lock_guard<std::mutex> lock(mu_);
            if (current->generation != generation_.load()) {
                current.reset();
                continue;
            }
            if (!uploaded) uploaded = upload_frame(current->data.get());
            if (uploaded) {
                if (!has_frame_.exchange(true) && first_frame_fn_) first_frame_fn_();
                hr = present_nv12();
            }
            if (hr == S_OK) {
                ++presented_count_;
            }
        }
        int64_t video_time = current->timestamp_ns;
        if (hr == S_OK || (FAILED(hr) && hr != DXGI_ERROR_WAS_STILL_DRAWING)) {
            current.reset();
        } else {
            if (hr == DXGI_ERROR_WAS_STILL_DRAWING) ++busy_count_;
            std::unique_lock<std::mutex> lock(frame_mu_);
            frame_cv_.wait_for(lock, std::chrono::milliseconds(hr == DXGI_STATUS_OCCLUDED ? 16 : 1));
        }
        auto now = std::chrono::steady_clock::now();
        if (now - last_stats >= std::chrono::seconds(5)) {
            size_t encoded, pending;
            uint64_t discarded;
            {
                std::lock_guard<std::mutex> lock(q_mu_);
                encoded = q_.size();
            }
            {
                std::lock_guard<std::mutex> lock(frame_mu_);
                pending = frames_.size();
                discarded = frames_.discarded();
            }
            double seconds = std::chrono::duration<double>(now - last_stats).count();
            uint64_t received = received_count_.load();
            uint64_t bytes = received_bytes_.load();
            uint64_t decoded = decoded_count_.load();
            char line[448];
            snprintf(line, sizeof(line),
                     "video stats: size=%dx%d input_fps=%.1f decode_fps=%.1f input_mbps=%.2f decoded=%llu presented=%llu skipped=%llu busy=%llu encoded=%zu pending=%zu max_decode_us=%llu max_submit_wait_us=%llu av_delta_ms=%lld",
                     width_.load(), height_.load(), (received - last_received) / seconds,
                     (decoded - last_decoded) / seconds, (bytes - last_bytes) * 8.0 / seconds / 1000000,
                     (unsigned long long) decoded, (unsigned long long) presented_count_.load(),
                     (unsigned long long) discarded, (unsigned long long) busy_count_.load(), encoded, pending,
                     (unsigned long long) max_decode_us_.exchange(0), (unsigned long long) max_submit_wait_us_.exchange(0),
                     (long long) (audio_time && video_time ? (video_time - audio_time) / 1000000 : 0));
            append_log(line);
            last_stats = now;
            last_received = received;
            last_bytes = bytes;
            last_decoded = decoded;
        }
    }
    if (mmcss) AvRevertMmThreadCharacteristics(mmcss);
}

void VideoPipeline::upload_yuv420(const uint8_t *y, int y_stride, const uint8_t *u, const uint8_t *v, int uv_stride, int w, int h) {
    if (!ensure_staging(w, h)) {
        return;
    }
    D3D11_MAPPED_SUBRESOURCE map = {};
    if (SUCCEEDED(ctx_->Map(staging_, 0, D3D11_MAP_WRITE, 0, &map))) {
        for (int row = 0; row < h; ++row) {
            memcpy((uint8_t *) map.pData + row * map.RowPitch, y + row * y_stride, w);
        }
        uint8_t *dst_uv = (uint8_t *) map.pData + map.RowPitch * h;
        for (int row = 0; row < h / 2; ++row) {
            uint8_t *dst = dst_uv + row * map.RowPitch;
            const uint8_t *su = u + row * uv_stride;
            const uint8_t *sv = v + row * uv_stride;
            for (int x = 0; x < w / 2; ++x) {
                dst[x * 2] = su[x];
                dst[x * 2 + 1] = sv[x];
            }
        }
        ctx_->Unmap(staging_, 0);
        ctx_->CopyResource(nv12_, staging_);
    }
}

HRESULT VideoPipeline::present_nv12() {
    if (!rtv_ || !swapchain_) {
        return E_FAIL;
    }
    RECT rc = {};
    GetClientRect(hwnd_, &rc);
    int cw = rc.right - rc.left;
    int ch = rc.bottom - rc.top;
    if (cw <= 0 || ch <= 0) {
        return DXGI_STATUS_OCCLUDED;
    }

    D3D11_VIEWPORT vp = {};
    vp.Width = (float) cw;
    vp.Height = (float) ch;
    vp.MaxDepth = 1;
    ctx_->RSSetViewports(1, &vp);
    ctx_->OMSetRenderTargets(1, &rtv_, nullptr);
    float black[4] = {0, 0, 0, 1};
    ctx_->ClearRenderTargetView(rtv_, black);

    float sx = 1, sy = 1;
    int src_w = width_.load();
    int src_h = height_.load();
    if (src_w > 0 && src_h > 0) {
        float va = (float) src_w / (float) src_h;
        float wa = (float) cw / (float) ch;
        if (cover_.load()) {
            if (wa > va) {
                sy = wa / va;
            } else {
                sx = va / wa;
            }
        } else if (wa > va) {
            sx = va / wa;
        } else {
            sy = wa / va;
        }
    }

    if (cbuf_ && (cw != present_cw_ || ch != present_ch_ || sx != letter_x_ || sy != letter_y_)) {
        D3D11_MAPPED_SUBRESOURCE map = {};
        if (SUCCEEDED(ctx_->Map(cbuf_, 0, D3D11_MAP_WRITE_DISCARD, 0, &map))) {
            float *f = (float *) map.pData;
            f[0] = sx;
            f[1] = sy;
            f[2] = 0;
            f[3] = 0;
            ctx_->Unmap(cbuf_, 0);
        }
        present_cw_ = cw;
        present_ch_ = ch;
        letter_x_ = sx;
        letter_y_ = sy;
    }
    if (cbuf_) {
        ctx_->VSSetConstantBuffers(0, 1, &cbuf_);
    }
    if (color_cbuf_ && color_cbuf_dirty_) {
        D3D11_MAPPED_SUBRESOURCE map = {};
        if (SUCCEEDED(ctx_->Map(color_cbuf_, 0, D3D11_MAP_WRITE_DISCARD, 0, &map))) {
            float *f = (float *) map.pData;
            f[0] = full_range_ ? 1.0f : 0.0f;
            f[1] = 0;
            f[2] = 0;
            f[3] = 0;
            ctx_->Unmap(color_cbuf_, 0);
        }
        color_cbuf_dirty_ = false;
    }
    if (color_cbuf_) {
        ctx_->PSSetConstantBuffers(1, 1, &color_cbuf_);
    }

    UINT stride = sizeof(Vertex), offset = 0;
    ctx_->IASetInputLayout(layout_);
    ctx_->IASetVertexBuffers(0, 1, &vb_, &stride, &offset);
    ctx_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    ctx_->VSSetShader(vs_, nullptr, 0);
    ctx_->PSSetShader(ps_, nullptr, 0);
    ctx_->PSSetSamplers(0, 1, &sampler_);
    ID3D11ShaderResourceView *srvs[2] = {srv_y_, srv_uv_};
    ctx_->PSSetShaderResources(0, 2, srvs);
    ctx_->RSSetState(raster_);
    ctx_->Draw(4, 0);
    ID3D11ShaderResourceView *none[2] = {};
    ctx_->PSSetShaderResources(0, 2, none);

    HRESULT hr = swapchain_->Present(0, DXGI_PRESENT_DO_NOT_WAIT | (tearing_ ? DXGI_PRESENT_ALLOW_TEARING : 0));
    if (hr == DXGI_ERROR_INVALID_CALL && tearing_) {
        tearing_ = false;
        hr = swapchain_->Present(0, DXGI_PRESENT_DO_NOT_WAIT);
    }
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        append_log("DXGI device lost during present");
    }
    return hr;
}

} // namespace airscreen
