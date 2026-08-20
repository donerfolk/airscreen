#pragma once

#include <string>
#include <cstdint>

namespace airscreen {

struct Settings {
    std::string name = "AirScreen";
    bool always_on_top = false;
    bool require_pin = false;
    bool fill_screen = true;
    int pin = 0; // 0 = random each time UxPlay assigns
    int width = 1920;
    int height = 1080;
    int refresh_hz = 60;
    int max_fps = 60;
};

std::wstring app_data_dir();
std::string app_data_dir_utf8();
std::string settings_path();
std::string keyfile_path();
std::string log_path();

Settings load_settings();
void save_settings(const Settings &s);
void append_log(const char *msg);

std::string find_mac_address();
std::string random_mac_address();

bool ensure_firewall_rule();

} // namespace airscreen
