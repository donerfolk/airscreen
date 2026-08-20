#pragma once

#include "settings.h"
#include "video.h"
#include "audio.h"

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

struct HWND__;
typedef HWND__ *HWND;

namespace airscreen {

enum class UiEvent {
    Connected,
    Disconnected,
    Pin,
    Log,
    VideoSize,
    VideoReady,
    ClientName,
};

class Receiver {
public:
    using UiFn = std::function<void(UiEvent, const std::string &)>;

    Receiver();
    ~Receiver();

    bool start(HWND hwnd, const Settings &settings, UiFn ui);
    void stop();
    bool rename(const std::string &name);
    bool set_require_pin(bool on);
    void set_always_on_top_flag(bool on) { (void) on; }

    VideoPipeline &video() { return video_; }
    bool connected() const { return connected_; }
    std::string pin() const;
    std::string client_name() const;
    std::string receiver_name() const { return name_; }

    // callbacks (public so C thunks can reach them)
    void on_audio(void *ntp, void *data);
    void on_video(void *ntp, void *data);
    void on_conn_init();
    void on_conn_destroy();
    void on_reset(int reason);
    void on_pin(const char *pin);
    void on_format(unsigned char ct, unsigned short spf, bool using_screen);
    void on_size(float w, float h);
    void on_volume(float db);
    void on_flush_audio();
    void on_flush_video();
    void on_client(const char *name);
    void on_log(int level, const char *msg);

private:
    bool start_dnssd();
    bool start_raop();
    void apply_features();
    void teardown_session();
    std::vector<char> hw_bytes_;
    std::atomic<int> open_conns_{0};

    HWND hwnd_ = nullptr;
    Settings settings_;
    UiFn ui_;
    std::string name_;
    std::string mac_;
    std::string keyfile_;
    std::string pin_;
    std::string client_;
    std::atomic<bool> connected_{false};
    unsigned char pin_pw_ = 0;

    void *dnssd_ = nullptr;
    void *raop_ = nullptr;
    unsigned short raop_port_ = 0;

    VideoPipeline video_;
    AudioPipeline audio_;
    std::mutex pin_mu_;
    uint64_t clock_offset_ = 0;
};

} // namespace airscreen
