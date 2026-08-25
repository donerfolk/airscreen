# AirScreen

Free Windows AirPlay receiver. Your iPhone lists the PC under **Control Center → Screen Mirroring**. Original app (GPLv3) built on the open-source [UxPlay](https://github.com/FDH2/UxPlay) protocol stack with a low-latency D3D11 / WASAPI renderer (no GStreamer).

## Use

1. PC and iPhone on the same Wi-Fi (not guest / AP-isolation).
2. Run `AirScreen.exe`. Allow the firewall if Windows asks.
3. iPhone: Control Center → Screen Mirroring → **AirScreen**.
4. Tray icon or idle window: rename, always-on-top, pairing PIN, fullscreen.

Typical latency on 5 GHz Wi-Fi is about 80–150 ms (iPhone encode + network). AirScreen keeps the PC side to hardware decode and a short present path.

## Features

- Appears as an AirPlay Screen Mirroring target via mDNS / RAOP
- Continuity-style idle window (GDI+); mirrored video via Direct3D 11
- Hardware H.264 / HEVC decode through FFmpeg (D3D11VA when available)
- Audio via WASAPI shared mode
- Rename receiver, optional pairing PIN (iOS 16+), always-on-top, cover/letterbox, fullscreen
- Single-instance tray app; close hides to the tray
- Windows Firewall helper from the tray menu
- Settings and pairing keys under `%LOCALAPPDATA%\AirScreen`

## Build (Windows 10/11 x64)

Needs Visual Studio 2022 with “Desktop development with C++”, CMake, OpenSSL, FFmpeg.

```powershell
winget install Kitware.CMake
winget install ShiningLight.OpenSSL.Dev
powershell -ExecutionPolicy Bypass -File scripts\fetch-deps.ps1

cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Or: `powershell -ExecutionPolicy Bypass -File scripts\build.ps1`

`AirScreen.exe` lands in `build\Release\` with FFmpeg/OpenSSL DLLs copied beside it.

Installer: compile `scripts\airscreen.iss` with [Inno Setup](https://jrsoftware.org/isinfo.php) after a Release build.

### Dependencies

| Dependency | Role |
|---|---|
| OpenSSL 1.1.1+ | Pairing / crypto (UxPlay) |
| FFmpeg | H.264 / HEVC decode |
| libplist (vendored) | AirPlay plists |
| UxPlay lib (vendored) | RAOP, mirror RTP, mDNS, FairPlay unwrap |
| Windows SDK | Win32, D3D11, DXGI, WASAPI, GDI+, DWM |

OpenSSL is resolved from `OPENSSL_ROOT_DIR`, Shining Light install paths, or vcpkg. FFmpeg comes from `scripts\fetch-deps.ps1` into `third_party/prebuilt`.

---

## Documentation

### Architecture

```
iPhone (Screen Mirroring)
        │  mDNS + RAOP / AirPlay mirror
        ▼
┌─────────────────────────────────────────┐
│  third_party/uxplay  (RAOP, RTP, mDNS)  │
│  fairplay / playfair, llhttp, dnssd     │
└──────────────────┬──────────────────────┘
                   │ callbacks
                   ▼
┌─────────────────────────────────────────┐
│  src/receiver.cpp   Receiver            │
│  · start/stop dnssd + raop              │
│  · PIN, rename, session teardown        │
│  · fan-out to video + audio pipelines   │
└──────────┬──────────────────┬───────────┘
           │                  │
           ▼                  ▼
┌──────────────────┐  ┌──────────────────┐
│ src/video.cpp    │  │ src/audio.cpp    │
│ FFmpeg decode    │  │ AAC / ALAC →     │
│ NV12 → D3D11     │  │ WASAPI render    │
│ swapchain present│  └──────────────────┘
└────────┬─────────┘
         ▼
┌─────────────────────────────────────────┐
│  src/main.cpp + src/ui.cpp              │
│  Win32 window, tray, idle Continuity UI │
│  settings (src/settings.cpp)            │
└─────────────────────────────────────────┘
```

**Idle path:** `ShellUi` paints a frosted Continuity plate (receiver name, status, PIN, toolbar). Video has not started.

**Live path:** first decoded frame flips the window to the D3D11 swapchain; audio plays on the default WASAPI device. Disconnect restores the idle plate.

### Source map

| Path | Purpose |
|---|---|
| `src/main.cpp` | Entry, window/tray, message loop, DPI, priority / timer period |
| `src/ui.cpp` / `ui.h` | Idle Continuity shell (GDI+), hit-testing, icons |
| `src/receiver.cpp` / `receiver.h` | UxPlay RAOP/mDNS bridge, UI events, session state |
| `src/video.cpp` / `video.h` | Decode worker queue, D3D11VA / software path, present |
| `src/audio.cpp` / `audio.h` | Decode + WASAPI output, volume / flush |
| `src/settings.cpp` / `settings.h` | Persist settings, MAC, firewall rule, log path |
| `src/app/` | `app.ico`, `logo.png`, resources |
| `third_party/uxplay/` | Protocol stack (Windows-ported sockets / netutils) |
| `third_party/libplist/` | Binary/XML plist |
| `third_party/win_compat/` | POSIX shims for UxPlay on Win32 |
| `scripts/` | `fetch-deps.ps1`, `build.ps1`, `airscreen.iss`, `make-icon.py` |
| `cmake/` | `FindFFmpeg.cmake` and helpers |

### Settings (`%LOCALAPPDATA%\AirScreen`)

| File | Contents |
|---|---|
| `settings.json` (or equivalent via `settings_path()`) | Name, always-on-top, require PIN, fill/cover, resolution hints, max FPS |
| Keyfile (`keyfile_path()`) | Persistent pairing material for UxPlay |
| Log (`log_path()`) | Diagnostic append log |

Defaults (see `Settings` in `src/settings.h`): name `AirScreen`, fill screen on, 1920×1080 @ 60, max FPS 60, PIN off (random when required).

### UI events

`Receiver` posts to the UI thread via `UiEvent`:

| Event | Meaning |
|---|---|
| `Connected` / `Disconnected` | Session lifetime |
| `Pin` | Show pairing code |
| `VideoSize` / `VideoReady` | Layout and first-frame swap to D3D11 |
| `ClientName` | Connected device label |
| `Log` | Status / debug line |

### Video pipeline notes

- Encoded NAL units enqueue on a worker thread (`kMaxQueued`); decode does not block the RAOP callback.
- Prefers D3D11 hardware decode; falls back to software YUV upload into an NV12 texture.
- Presents with letterbox or cover (`set_cover`); optional DXGI tearing when supported.
- Process boosts (1 ms timer resolution, high priority class, optional power-throttling opt-out) reduce present jitter on Windows.

### Network / discovery

- mDNS advertises the receiver name and AirPlay features.
- Same L2/L3 subnet as the iPhone; VPN and AP client isolation commonly hide the PC.
- Firewall: inbound TCP/UDP for the RAOP / mirror ports — use tray → allow through firewall if discovery fails.

### Design


### Troubleshooting

- **Receiver missing on iPhone:** same subnet, disable VPN, allow AirScreen in Windows Firewall (tray → Allow through firewall).
- **iOS asks for a PIN:** tray or idle toolbar → Require pairing PIN, or type the on-screen code.
- **No audio:** default Windows playback device; avoid exclusive-mode apps hogging WASAPI.
- **Stutter / high latency:** prefer 5 GHz Wi-Fi; close GPU-heavy overlays; ensure FFmpeg D3D11VA is in use (check log).
- **Black window after connect:** wait for first IDR; if stuck, stop mirroring and reconnect; check `log_path()` for decode errors.

### License

GPLv3. See [LICENSE](LICENSE) and [NOTICE](NOTICE).

Third-party: UxPlay / playfair (GPLv3), llhttp (MIT), libplist (LGPL-2.1), FFmpeg (GPL), OpenSSL (Apache-2.0).
