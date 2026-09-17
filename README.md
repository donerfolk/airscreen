<p align="center">
  <img src="src/app/logo.png" width="96" alt="AirScreen logo">
</p>

<h1 align="center">AirScreen</h1>

<p align="center">
  Mirror your iPhone to a Windows PC over AirPlay. Free and open source.
</p>

---

AirScreen turns a Windows PC into an AirPlay receiver. Your iPhone lists it under **Control Center → Screen Mirroring**, just like an Apple TV. Video and audio play in a resizable window or full screen.

## Features

- Shows up as a Screen Mirroring target, no app needed on the iPhone
- Hardware-accelerated H.264 and HEVC decoding (Direct3D 11)
- Audio plays through your default Windows output device
- Rename the receiver, require a pairing PIN, keep the window on top, fill the screen or letterbox
- Light and dark idle screen
- Runs from the system tray; closing the window keeps the receiver available

## Requirements

- Windows 10 or 11, 64-bit
- iPhone and PC on the same network. Guest networks and routers with client isolation will hide the PC.

AirScreen is tested with iPhone. iPad and Mac use the same protocol and may work, but are untested.

## Install

Download the latest installer from [Releases](https://github.com/donerfolk/airscreen/releases), or [build from source](#building-from-source).

## Usage

1. Start AirScreen. If Windows Firewall asks, allow access on private networks.
2. On your iPhone, open **Control Center → Screen Mirroring** and pick **AirScreen**.
3. If a PIN appears on the PC, enter it on the iPhone.

To stop, end mirroring on the iPhone. Right-click the tray icon for settings or to exit.

## Troubleshooting

**AirScreen doesn't appear on the iPhone**
- Make sure both devices are on the same Wi-Fi network (not a guest network).
- Turn off any VPN on the PC or the iPhone.
- Right-click the tray icon → **Allow through firewall**.

**Stuttering or delay.** Use 5 GHz Wi-Fi or connect the PC by Ethernet. Other devices streaming on the same network can add delay.

**No sound.** Check the default playback device in Windows sound settings. Apps that take exclusive control of the device can block AirScreen.

**Black window after connecting.** Stop mirroring and connect again. If it keeps happening, right-click the tray icon → **Open log file** and include the log in a [bug report](https://github.com/donerfolk/airscreen/issues).

Settings, pairing keys and the log live in `%APPDATA%\AirScreen`.

## Building from source

Requires Visual Studio 2022 with the *Desktop development with C++* workload.

```powershell
winget install Kitware.CMake
winget install ShiningLight.OpenSSL.Dev
powershell -ExecutionPolicy Bypass -File scripts\build.ps1
```

`build.ps1` downloads FFmpeg into `third_party\prebuilt`, configures CMake and builds `build\Release\AirScreen.exe` with the required DLLs next to it. OpenSSL is found through `OPENSSL_ROOT_DIR`, the Shining Light install location, or vcpkg.

To build an installer, compile `scripts\airscreen.iss` with [Inno Setup](https://jrsoftware.org/isinfo.php) after a Release build.

### Project layout

| Path | Contents |
|---|---|
| `src/receiver.cpp` | AirPlay session handling on top of UxPlay |
| `src/video.cpp` | FFmpeg decoding and Direct3D 11 presentation |
| `src/audio.cpp` | AAC/ALAC decoding and WASAPI playback |
| `src/main.cpp`, `src/ui.cpp` | Window, tray menu and idle screen |
| `src/settings.cpp` | Settings file, firewall rule, log |
| `third_party/uxplay` | AirPlay protocol stack (RAOP, mirroring, mDNS) |
| `third_party/libplist` | Property list parsing |

## Credits

AirScreen's protocol support comes from [UxPlay](https://github.com/FDH2/UxPlay) and the projects it builds on (RPiPlay, shairplay, playfair). It also uses [FFmpeg](https://ffmpeg.org/), [OpenSSL](https://www.openssl.org/), [libplist](https://github.com/libimobiledevice/libplist) and llhttp.

AirScreen is not affiliated with Apple. AirPlay and iPhone are trademarks of Apple Inc.

## License

[GPL-3.0](LICENSE). Third-party licenses are listed in [NOTICE](NOTICE).
