#pragma once

#include <d3d11.h>
#include <dxgi1_2.h>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

struct HWND__;
typedef HWND__ *HWND;

namespace airscreen {

class VideoPipeline {
public:
    VideoPipeline();
    ~VideoPipeline();

    bool init(HWND hwnd);
    void shutdown();
    void resize();
    void flush();

    void submit(const uint8_t *data, int len, uint64_t ntp_ns, bool hevc);
    void set_source_size(int width, int height);
    void set_cover(bool cover);
    void set_first_frame_fn(std::function<void()> fn);
    void clear();

    int width() const { return width_.load(); }
    int height() const { return height_.load(); }
    bool has_frame() const { return has_frame_.load(); }

    ID3D11Device *device() const { return device_; }

private:
    struct EncodedPacket {
        std::vector<uint8_t> data;
        uint64_t ntp_ns = 0;
        bool hevc = false;
    };

    bool create_device();
    bool create_swapchain();
    bool create_pipeline();
    bool ensure_nv12(int w, int h);
    bool ensure_staging(int w, int h);
    bool decode_init(bool hevc);
    void decode_close();
    void decode_packet(const EncodedPacket &pkt);
    void decode_loop();
    void start_worker();
    void stop_worker();
    void present_nv12();
    void upload_yuv420(const uint8_t *y, int y_stride, const uint8_t *u, const uint8_t *v, int uv_stride, int w, int h);

    HWND hwnd_ = nullptr;
    ID3D11Device *device_ = nullptr;
    ID3D11DeviceContext *ctx_ = nullptr;
    IDXGISwapChain1 *swapchain_ = nullptr;
    ID3D11RenderTargetView *rtv_ = nullptr;
    ID3D11VertexShader *vs_ = nullptr;
    ID3D11PixelShader *ps_ = nullptr;
    ID3D11InputLayout *layout_ = nullptr;
    ID3D11Buffer *vb_ = nullptr;
    ID3D11SamplerState *sampler_ = nullptr;
    ID3D11Texture2D *nv12_ = nullptr;
    ID3D11ShaderResourceView *srv_y_ = nullptr;
    ID3D11ShaderResourceView *srv_uv_ = nullptr;
    ID3D11RasterizerState *raster_ = nullptr;
    ID3D11Buffer *cbuf_ = nullptr;
    ID3D11Texture2D *staging_ = nullptr;

    void *av_hw_device_ = nullptr;
    void *av_codec_ = nullptr;
    void *av_frame_ = nullptr;
    void *av_pkt_ = nullptr;
    bool hevc_ = false;
    bool tearing_ = false;

    std::atomic<int> width_{0};
    std::atomic<int> height_{0};
    int tex_w_ = 0;
    int tex_h_ = 0;
    int staging_w_ = 0;
    int staging_h_ = 0;
    int present_cw_ = 0;
    int present_ch_ = 0;
    float letter_x_ = 0;
    float letter_y_ = 0;
    std::atomic<bool> has_frame_{false};
    std::atomic<bool> cover_{false};
    std::mutex mu_;
    std::function<void()> first_frame_fn_;

    static constexpr size_t kMaxQueued = 6;
    std::mutex q_mu_;
    std::condition_variable q_cv_;
    std::deque<EncodedPacket> q_;
    std::atomic<bool> worker_run_{false};
    std::thread worker_;
};

} // namespace airscreen
