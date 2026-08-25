#include "receiver.h"

#include "logger.h"
#include "raop.h"
#include "stream.h"
#include "dnssd.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstring>
#include <cstdlib>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <mutex>

namespace airscreen {
namespace {

std::mutex g_log_mu;
std::ofstream g_log;

void file_log(const char *msg, bool flush) {
    std::lock_guard<std::mutex> lock(g_log_mu);
    if (!g_log.is_open()) {
        g_log.open(log_path(), std::ios::app);
    }
    if (g_log) {
        SYSTEMTIME st = {};
        GetLocalTime(&st);
        char ts[32];
        snprintf(ts, sizeof(ts), "%02u:%02u:%02u.%03u ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
        g_log << ts << msg << '\n';
        if (flush) {
            g_log.flush();
        }
    }
}

Receiver *self(void *cls) {
    return static_cast<Receiver *>(cls);
}

extern "C" void cb_audio(void *cls, raop_ntp_t *ntp, audio_decode_struct *data) {
    self(cls)->on_audio(ntp, data);
}
extern "C" void cb_video(void *cls, raop_ntp_t *ntp, video_decode_struct *data) {
    self(cls)->on_video(ntp, data);
}
extern "C" void cb_conn_init(void *cls) {
    self(cls)->on_conn_init();
}
extern "C" void cb_conn_destroy(void *cls) {
    self(cls)->on_conn_destroy();
}
extern "C" void cb_pin(void *cls, char *pin) {
    self(cls)->on_pin(pin);
}
extern "C" void cb_format(void *cls, unsigned char *ct, unsigned short *spf, bool *usingScreen, bool *isMedia,
                          uint64_t *audioFormat) {
    (void) isMedia;
    (void) audioFormat;
    self(cls)->on_format(*ct, *spf, usingScreen ? *usingScreen : false);
}
extern "C" void cb_size(void *cls, float *width_source, float *height_source, float *width, float *height) {
    (void) width;
    (void) height;
    self(cls)->on_size(*width_source, *height_source);
}
extern "C" void cb_volume(void *cls, float volume) {
    self(cls)->on_volume(volume);
}
extern "C" double cb_client_volume(void *cls) {
    (void) cls;
    return 0.0;
}
extern "C" void cb_flush_a(void *cls) {
    self(cls)->on_flush_audio();
}
extern "C" void cb_flush_v(void *cls) {
    self(cls)->on_flush_video();
}
extern "C" void cb_client(void *cls, char *deviceid, char *model, char *name, bool *admit) {
    (void) deviceid;
    (void) model;
    *admit = true;
    self(cls)->on_client(name ? name : "");
}
extern "C" int cb_codec(void *cls, video_codec_t codec) {
    (void) cls;
    (void) codec;
    return 0;
}
extern "C" bool cb_check_reg(void *cls, const char *pk) {
    (void) cls;
    (void) pk;
    return true;
}
extern "C" void cb_reg(void *cls, const char *id, const char *pk, const char *name) {
    (void) cls;
    (void) id;
    (void) pk;
    (void) name;
}
extern "C" const char *cb_passwd(void *cls, int *len) {
    (void) cls;
    *len = 0;
    return nullptr;
}
extern "C" void cb_dacp(void *cls, const char *a, const char *b) {
    (void) cls;
    (void) a;
    (void) b;
}
extern "C" void cb_log(void *cls, int level, const char *msg) {
    self(cls)->on_log(level, msg);
}
extern "C" void cb_nop(void *cls) {
    (void) cls;
}
extern "C" void cb_reset(void *cls, int reason) {
    self(cls)->on_reset(reason);
}
extern "C" void cb_feedback(void *cls) {
    (void) cls;
}
extern "C" void cb_video_reset(void *cls, reset_type_t t) {
    (void) t;
    self(cls)->on_flush_video();
}

void parse_mac(const std::string &mac, std::vector<char> &out) {
    out.clear();
    for (size_t i = 0; i + 1 < mac.size(); i += 3) {
        out.push_back((char) strtol(mac.c_str() + i, nullptr, 16));
    }
}

} // namespace

Receiver::Receiver() = default;

Receiver::~Receiver() {
    stop();
}

bool Receiver::start(HWND hwnd, const Settings &settings, UiFn ui) {
    stop();
    hwnd_ = hwnd;
    settings_ = settings;
    ui_ = std::move(ui);
    name_ = settings_.name.empty() ? "AirScreen" : settings_.name;
    pin_pw_ = settings_.require_pin ? 1 : 0;
    keyfile_ = keyfile_path();
    mac_ = find_mac_address();
    if (mac_.empty()) {
        mac_ = random_mac_address();
    }
    parse_mac(mac_, hw_bytes_);
    ntp_global_init();
    on_log(LOGGER_INFO, "starting D3D11");
    if (!video_.init(hwnd_)) {
        on_log(LOGGER_ERR, "D3D11 init failed");
        return false;
    }
    video_.set_first_frame_fn([this] {
        if (!connected_.load()) {
            return;
        }
        SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED);
        if (ui_) {
            ui_(UiEvent::VideoReady, {});
        }
    });
    on_log(LOGGER_INFO, "starting mDNS");
    if (!start_dnssd()) {
        stop();
        return false;
    }
    on_log(LOGGER_INFO, "starting RAOP");
    if (!start_raop()) {
        stop();
        return false;
    }
    on_log(LOGGER_INFO, ("AirScreen ready as " + name_ + " mac=" + mac_).c_str());
    return true;
}

void Receiver::stop() {
    open_conns_ = 0;
    if (raop_) {
        raop_stop_httpd((raop_t *) raop_);
        raop_destroy((raop_t *) raop_);
        raop_ = nullptr;
    }
    if (dnssd_) {
        dnssd_unregister_raop((dnssd_t *) dnssd_);
        dnssd_unregister_airplay((dnssd_t *) dnssd_);
        dnssd_destroy((dnssd_t *) dnssd_);
        dnssd_ = nullptr;
    }
    audio_.stop();
    video_.shutdown();
    connected_ = false;
}

bool Receiver::rename(const std::string &name) {
    if (name.empty()) {
        return false;
    }
    name_ = name;
    settings_.name = name;
    save_settings(settings_);
    if (dnssd_) {
        dnssd_unregister_raop((dnssd_t *) dnssd_);
        dnssd_unregister_airplay((dnssd_t *) dnssd_);
        dnssd_destroy((dnssd_t *) dnssd_);
        dnssd_ = nullptr;
    }
    if (!start_dnssd()) {
        return false;
    }
    if (raop_) {
        raop_set_dnssd((raop_t *) raop_, (dnssd_t *) dnssd_);
        dnssd_register_raop((dnssd_t *) dnssd_, raop_port_);
        dnssd_register_airplay((dnssd_t *) dnssd_, raop_port_);
    }
    return true;
}

bool Receiver::set_require_pin(bool on) {
    settings_.require_pin = on;
    pin_pw_ = on ? 1 : 0;
    save_settings(settings_);
    return rename(name_);
}

bool Receiver::start_dnssd() {
    int err = 0;
    auto *ds = dnssd_init(name_.c_str(), (int) name_.size(), hw_bytes_.data(), (int) hw_bytes_.size(), pin_pw_, &err);
    if (!ds || err) {
        on_log(LOGGER_ERR, "dnssd_init failed");
        return false;
    }
    dnssd_ = ds;
    apply_features();
    return true;
}

void Receiver::apply_features() {
    auto *ds = (dnssd_t *) dnssd_;
    dnssd_set_airplay_features(ds, 0, 0);
    dnssd_set_airplay_features(ds, 1, 1);
    dnssd_set_airplay_features(ds, 2, 1);
    dnssd_set_airplay_features(ds, 3, 0);
    dnssd_set_airplay_features(ds, 4, 0);
    dnssd_set_airplay_features(ds, 5, 1);
    dnssd_set_airplay_features(ds, 6, 1);
    dnssd_set_airplay_features(ds, 7, 1);  // mirroring
    dnssd_set_airplay_features(ds, 8, 0);
    dnssd_set_airplay_features(ds, 9, 1);  // audio
    dnssd_set_airplay_features(ds, 10, 1);
    dnssd_set_airplay_features(ds, 11, 1);
    dnssd_set_airplay_features(ds, 12, 1);
    dnssd_set_airplay_features(ds, 13, 1);
    dnssd_set_airplay_features(ds, 14, 1);
    dnssd_set_airplay_features(ds, 15, 1);
    dnssd_set_airplay_features(ds, 16, 1);
    dnssd_set_airplay_features(ds, 17, 1);
    dnssd_set_airplay_features(ds, 18, 1);
    dnssd_set_airplay_features(ds, 19, 1);
    dnssd_set_airplay_features(ds, 20, 1);
    dnssd_set_airplay_features(ds, 21, 1);
    dnssd_set_airplay_features(ds, 22, 1);
    dnssd_set_airplay_features(ds, 23, 0);
    dnssd_set_airplay_features(ds, 24, 0);
    dnssd_set_airplay_features(ds, 25, 1);
    dnssd_set_airplay_features(ds, 26, 0);
    dnssd_set_airplay_features(ds, 27, 1);
    dnssd_set_airplay_features(ds, 28, 1);
    dnssd_set_airplay_features(ds, 29, 0);
    dnssd_set_airplay_features(ds, 30, 1);
    dnssd_set_airplay_features(ds, 31, 0);
    dnssd_set_airplay_features(ds, 42, 0); // h265 off — h264 only for MVP
}

bool Receiver::start_raop() {
    raop_callbacks_t cbs;
    memset(&cbs, 0, sizeof(cbs));
    cbs.cls = this;
    cbs.audio_process = cb_audio;
    cbs.video_process = cb_video;
    cbs.conn_init = cb_conn_init;
    cbs.conn_destroy = cb_conn_destroy;
    cbs.conn_reset = cb_reset;
    cbs.conn_feedback = cb_feedback;
    cbs.audio_flush = cb_flush_a;
    cbs.video_flush = cb_flush_v;
    cbs.audio_set_client_volume = cb_client_volume;
    cbs.audio_set_volume = cb_volume;
    cbs.audio_get_format = cb_format;
    cbs.video_report_size = cb_size;
    cbs.display_pin = cb_pin;
    cbs.report_client_request = cb_client;
    cbs.register_client = cb_reg;
    cbs.check_register = cb_check_reg;
    cbs.passwd = cb_passwd;
    cbs.export_dacp = cb_dacp;
    cbs.video_set_codec = cb_codec;
    cbs.video_reset = cb_video_reset;
    cbs.video_pause = cb_nop;
    cbs.video_resume = cb_nop;

    raop_t *raop = raop_init(&cbs);
    if (!raop) {
        on_log(LOGGER_ERR, "raop_init failed");
        return false;
    }
    raop_set_log_callback(raop, cb_log, this);
    raop_set_log_level(raop, LOGGER_INFO);
    if (raop_init2(raop, 1, mac_.c_str(), keyfile_.c_str())) {
        on_log(LOGGER_ERR, "raop_init2 failed");
        raop_destroy(raop);
        return false;
    }
    raop_set_plist(raop, "width", settings_.width);
    raop_set_plist(raop, "height", settings_.height);
    raop_set_plist(raop, "refreshRate", settings_.refresh_hz);
    raop_set_plist(raop, "maxFPS", settings_.max_fps);
    if (pin_pw_ == 1 && settings_.pin) {
        raop_set_plist(raop, "pin", settings_.pin);
    }

    // Stable ports so iOS Control Center can reconnect after a crash.
    // tcp[0] = mirror data, tcp[1] = AirPlay HTTP (Apple TV uses 7000).
    unsigned short tcp[2] = {7100, 7000};
    unsigned short udp[3] = {7011, 6001, 6000};
    raop_set_tcp_ports(raop, tcp);
    raop_set_udp_ports(raop, udp);

    unsigned short port = raop_get_port(raop);
    on_log(LOGGER_INFO, "starting HTTP listener");
    if (raop_start_httpd(raop, &port) < 0) {
        on_log(LOGGER_WARNING, "TCP 7000 busy, falling back to an ephemeral port");
        port = 0;
        raop_set_port(raop, 0);
        if (raop_start_httpd(raop, &port) < 0) {
            on_log(LOGGER_ERR, "raop_start_httpd failed");
            raop_destroy(raop);
            return false;
        }
    }
    raop_set_port(raop, port);
    raop_port_ = port;
    raop_set_dnssd(raop, (dnssd_t *) dnssd_);
    raop_ = raop;

    int e1 = dnssd_register_raop((dnssd_t *) dnssd_, port);
    int e2 = dnssd_register_airplay((dnssd_t *) dnssd_, port);
    if (e1 || e2) {
        on_log(LOGGER_ERR, "mDNS register failed — check firewall / another AirPlay receiver");
        return false;
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "listening on TCP %u", port);
    on_log(LOGGER_INFO, buf);
    return true;
}

std::string Receiver::pin() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex &>(pin_mu_));
    return pin_;
}

std::string Receiver::client_name() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex &>(pin_mu_));
    return client_;
}

void Receiver::on_audio(void *ntp, void *data) {
    (void) ntp;
    auto *a = (audio_decode_struct *) data;
    if (!a || !a->data || a->data_len <= 0) {
        return;
    }
    if (!audio_.is_running()) {
        unsigned char ct = a->ct ? a->ct : 8;
        if (!audio_.start(44100, 2, ct)) {
            return;
        }
    }
    audio_.submit(a->data, a->data_len, a->ntp_time_remote);
}

void Receiver::on_video(void *ntp, void *data) {
    (void) ntp;
    auto *v = (video_decode_struct *) data;
    if (!v || !v->data || v->data_len <= 0) {
        return;
    }
    video_.submit(v->data, v->data_len, v->ntp_time_remote, v->is_h265);
}

void Receiver::on_conn_init() {
    int n = ++open_conns_;
    if (n == 1) {
        connected_ = true;
        clock_offset_ = 0;
        if (ui_) {
            ui_(UiEvent::Connected, client_);
        }
    }
}

void Receiver::teardown_session() {
    bool was = connected_.exchange(false);
    bool had_frame = video_.has_frame();
    on_log(LOGGER_INFO, "session teardown");
    audio_.stop();
    video_.flush();
    video_.clear();
    SetThreadExecutionState(ES_CONTINUOUS);
    {
        std::lock_guard<std::mutex> lock(pin_mu_);
        pin_.clear();
        client_.clear();
    }
    if ((was || had_frame) && ui_) {
        ui_(UiEvent::Disconnected, {});
    }
}

void Receiver::on_conn_destroy() {
    int n = --open_conns_;
    char buf[48];
    snprintf(buf, sizeof(buf), "conn_destroy open=%d", n);
    on_log(LOGGER_INFO, buf);
    if (n > 0) {
        return;
    }
    open_conns_ = 0;
    teardown_session();
}

void Receiver::on_reset(int reason) {
    char buf[48];
    snprintf(buf, sizeof(buf), "connection reset (%d)", reason);
    on_log(LOGGER_WARNING, buf);
    open_conns_ = 0;
    teardown_session();
}

void Receiver::on_pin(const char *pin) {
    {
        std::lock_guard<std::mutex> lock(pin_mu_);
        pin_ = pin ? pin : "";
    }
    if (ui_) {
        ui_(UiEvent::Pin, pin_);
    }
}

void Receiver::on_format(unsigned char ct, unsigned short spf, bool using_screen) {
    (void) using_screen;
    if (!audio_.start(44100, 2, ct)) {
        on_log(LOGGER_ERR, "audio pipeline failed to start");
        return;
    }
    char buf[80];
    snprintf(buf, sizeof(buf), "audio format ct=%u spf=%u", ct, spf);
    on_log(LOGGER_INFO, buf);
}

void Receiver::on_size(float w, float h) {
    video_.set_source_size((int) w, (int) h);
    if (ui_) {
        ui_(UiEvent::VideoSize, std::to_string((int) w) + "x" + std::to_string((int) h));
    }
}

void Receiver::on_volume(float db) {
    double frac = 0;
    if (db <= -30.f || db == -144.f) {
        frac = 0;
    } else if (db >= 0.f) {
        frac = 1;
    } else {
        frac = (30.0 + db) / 30.0;
    }
    audio_.set_volume((float) frac);
}

void Receiver::on_flush_audio() {
    audio_.flush();
}

void Receiver::on_flush_video() {
    video_.flush();
}

void Receiver::on_client(const char *name) {
    {
        std::lock_guard<std::mutex> lock(pin_mu_);
        client_ = name ? name : "";
    }
    if (ui_) {
        ui_(UiEvent::ClientName, client_);
    }
}

void Receiver::on_log(int level, const char *msg) {
    if (!msg) {
        return;
    }
    file_log(msg, level <= LOGGER_ERR);
    if (ui_ && level <= LOGGER_WARNING) {
        ui_(UiEvent::Log, msg);
    }
}

} // namespace airscreen
