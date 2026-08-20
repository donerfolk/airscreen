#pragma once

#include <d3d11.h>
#include <dxgi1_2.h>
#include <atomic>
#include <cstdint>
#include <mutex>

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
    void clear();

    int width() const { return width_; }
    int height() const { return height_; }
    bool has_frame() const { return has_frame_.load(); }

    ID3D11Device *device() const { return device_; }

private:
    bool create_device();
    bool create_swapchain();
    bool create_pipeline();
    bool ensure_nv12(int w, int h);
    bool decode_init(bool hevc);
    void decode_close();
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
    HANDLE waitable_ = nullptr;

    void *av_hw_device_ = nullptr;
    void *av_codec_ = nullptr;
    void *av_frame_ = nullptr;
    void *av_pkt_ = nullptr;
    bool hevc_ = false;

    int width_ = 0;
    int height_ = 0;
    int tex_w_ = 0;
    int tex_h_ = 0;
    std::atomic<bool> has_frame_{false};
    std::atomic<bool> cover_{false};
    std::mutex mu_;
};

} // namespace airscreen
