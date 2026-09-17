#include "settings.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <shlobj.h>
#include <shellapi.h>
#include <algorithm>
#include <cctype>
#include <string>
#include <fstream>
#include <random>
#include <cstdio>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "shell32.lib")

namespace airscreen {
namespace {

std::string wide_to_utf8(const std::wstring &w) {
    if (w.empty()) {
        return {};
    }
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int) w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int) w.size(), s.data(), n, nullptr, nullptr);
    return s;
}

std::string trim(std::string s) {
    auto notspace = [](unsigned char c) { return !isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notspace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notspace).base(), s.end());
    return s;
}

} // namespace

std::wstring app_data_dir() {
    wchar_t path[MAX_PATH] = {};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, path))) {
        return L".";
    }
    std::wstring dir = std::wstring(path) + L"\\AirScreen";
    CreateDirectoryW(dir.c_str(), nullptr);
    return dir;
}

std::string app_data_dir_utf8() {
    return wide_to_utf8(app_data_dir());
}

std::string settings_path() {
    return app_data_dir_utf8() + "\\settings.ini";
}

std::string keyfile_path() {
    return app_data_dir_utf8() + "\\airplay.key";
}

std::string log_path() {
    return app_data_dir_utf8() + "\\airscreen.log";
}

void append_log(const char *msg) {
    if (!msg) {
        return;
    }
    std::ofstream out(log_path(), std::ios::app);
    if (out) {
        out << msg << '\n';
        out.flush();
    }
}

Settings load_settings() {
    Settings s;
    std::ifstream in(settings_path());
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#' || line[0] == '[') {
            continue;
        }
        auto eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));
        if (key == "Name" && !val.empty()) {
            s.name = val;
        } else if (key == "AlwaysOnTop") {
            s.always_on_top = (val == "1" || val == "true");
        } else if (key == "RequirePin") {
            s.require_pin = (val == "1" || val == "true");
        } else if (key == "FillScreen") {
            s.fill_screen = (val == "1" || val == "true");
        } else if (key == "DarkMode") {
            s.dark_mode = (val == "1" || val == "true");
        } else if (key == "Width") {
            s.width = std::stoi(val);
        } else if (key == "Height") {
            s.height = std::stoi(val);
        } else if (key == "MaxFps") {
            s.max_fps = std::stoi(val);
        }
    }
    return s;
}

void save_settings(const Settings &s) {
    std::ofstream out(settings_path(), std::ios::trunc);
    out << "[AirScreen]\n";
    out << "Name=" << s.name << "\n";
    out << "AlwaysOnTop=" << (s.always_on_top ? 1 : 0) << "\n";
    out << "RequirePin=" << (s.require_pin ? 1 : 0) << "\n";
    out << "FillScreen=" << (s.fill_screen ? 1 : 0) << "\n";
    out << "DarkMode=" << (s.dark_mode ? 1 : 0) << "\n";
    out << "Width=" << s.width << "\n";
    out << "Height=" << s.height << "\n";
    out << "MaxFps=" << s.max_fps << "\n";
}

std::string find_mac_address() {
    ULONG buflen = sizeof(IP_ADAPTER_ADDRESSES);
    auto *addresses = (IP_ADAPTER_ADDRESSES *) malloc(buflen);
    if (!addresses) {
        return {};
    }
    if (GetAdaptersAddresses(AF_UNSPEC, 0, nullptr, addresses, &buflen) == ERROR_BUFFER_OVERFLOW) {
        free(addresses);
        addresses = (IP_ADAPTER_ADDRESSES *) malloc(buflen);
        if (!addresses) {
            return {};
        }
    }
    std::string mac;
    if (GetAdaptersAddresses(AF_UNSPEC, 0, nullptr, addresses, &buflen) == NO_ERROR) {
        for (auto *a = addresses; a; a = a->Next) {
            if (a->PhysicalAddressLength != 6 || a->OperStatus != IfOperStatusUp) {
                continue;
            }
            if (a->IfType != IF_TYPE_ETHERNET_CSMACD && a->IfType != IF_TYPE_IEEE80211) {
                continue;
            }
            char buf[32];
            snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x",
                     a->PhysicalAddress[0], a->PhysicalAddress[1], a->PhysicalAddress[2],
                     a->PhysicalAddress[3], a->PhysicalAddress[4], a->PhysicalAddress[5]);
            mac = buf;
            break;
        }
    }
    free(addresses);
    return mac;
}

std::string random_mac_address() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, 255);
    unsigned b0 = ((unsigned) dist(gen) & 0xFEu) | 0x02u; // locally administered unicast
    char buf[32];
    snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x",
             b0, dist(gen), dist(gen), dist(gen), dist(gen), dist(gen));
    return buf;
}

bool ensure_firewall_rule() {
    wchar_t exe[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    std::wstring cmd = L"advfirewall firewall add rule name=\"AirScreen\" dir=in action=allow program=\"";
    cmd += exe;
    cmd += L"\" enable=yes profile=any";
    SHELLEXECUTEINFOW sei = {};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
    sei.lpVerb = L"runas";
    sei.lpFile = L"netsh";
    sei.lpParameters = cmd.c_str();
    sei.nShow = SW_HIDE;
    if (!ShellExecuteExW(&sei)) {
        return false;
    }
    if (sei.hProcess) {
        WaitForSingleObject(sei.hProcess, 15000);
        CloseHandle(sei.hProcess);
    }
    return true;
}

} // namespace airscreen
