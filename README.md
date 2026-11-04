<p align="center">
  <img src="src/app/logo.png" width="96" alt="AirScreen logo">
</p>

<h1 align="center">AirScreen: Free AirPlay Receiver for Windows</h1>

<p align="center">
  Mirror your iPhone to a Windows PC over AirPlay. No Apple TV, no cables, no app to install on the iPhone. Free and open source.
</p>

<p align="center">
<a href="https://diestro.dev/tools/airscreen/"><strong>Setup guide and download: diestro.dev/tools/airscreen</strong></a>
</p>

<p align="center">
  <a href="https://buymeacoffee.com/donerfolk"><img src="https://cdn.buymeacoffee.com/buttons/v2/default-yellow.png" height="40" alt="Buy Me A Coffee"></a>
</p>

---

AirScreen turns a Windows PC into a wireless AirPlay receiver. Your iPhone lists it under **Control Center → Screen Mirroring**, just like an Apple TV, without needing one. Video and audio play in a resizable window or full screen, with no app to install on the iPhone.

## Features

- Shows up as a Screen Mirroring target, no app needed on the iPhone
- Hardware-accelerated H.264 decoding (Direct3D 11)
- Audio plays through your default Windows output device
- Rename the receiver, require a pairing PIN, keep the window on top, fill the screen or letterbox
- Light and dark idle screen
- Native-resolution window sizes (0.5x, 1x, 1.5x, 2x), optional fixed aspect ratio, and automatic portrait/landscape sizing
- Closing the window quits AirScreen; settings are also in the tray icon menu
- Optional Start with Windows: waits in the system tray, ready to mirror to

## Requirements

- Windows 10 or 11, 64-bit
- iOS 9.3 or later. Earlier versions speak an older AirPlay protocol that AirScreen doesn't implement, so they won't see the PC at all.
- iPhone and PC on the same network. Guest networks and routers with client isolation will hide the PC.

AirScreen is tested with iPhone. iPad and Mac use the same protocol and may work, but are untested.

## Install

Download the latest installer from [Releases](https://github.com/donerfolk/airscreen/releases), or [build from source](#building-from-source).

Every release is built from this repository by [GitHub Actions](https://github.com/donerfolk/airscreen/actions), with SHA-256 checksums in `SHA256SUMS.txt`.

The installer isn't code-signed yet, so Windows SmartScreen may show "Windows protected your PC". Click **More info → Run anyway**.

## Usage

1. Start AirScreen. If Windows Firewall asks, allow access on private networks.
2. On your iPhone, open **Control Center → Screen Mirroring** and pick **AirScreen**.
3. If a PIN appears on the PC, enter it on the iPhone.

To stop, end mirroring on the iPhone. Close the window to quit AirScreen. Right-click the window or the tray icon for settings.

**Streaming quality** defaults to 1080p at 30 FPS and also offers 1080p at 60 or 24 FPS, and 720p at 30 FPS (Smooth). Saved quality settings are preserved. Lower settings request a smaller stream from the iPhone, reducing network and decoding load. Changing quality restarts the receiver; reconnect the iPhone afterward. The iPhone chooses the actual stream dimensions and frame rate. For persistent playback delay, try 720p at 30 FPS before reducing audio buffering.

**Size** sets the picture area to **0.5x**, **1x**, **1.5x**, or **2x** the resolution reported by the mirrored device. Use **Ctrl+1**, **Ctrl+2**, **Ctrl+3**, or **Ctrl+4**, respectively. A size selected before connecting applies when the stream starts. Dragging an edge returns to a custom size. Enable **Fixed Ratio** to keep the original picture proportions while resizing. **Auto Rotate Window** is enabled by default and adjusts the window when the mirrored device switches between portrait and landscape; in full screen, it updates the window size restored on exit. Both options are saved.

## Troubleshooting

**AirScreen doesn't appear on the iPhone**
- On Windows, discovery uses a connected physical Ethernet or Wi-Fi adapter and its current IPv4 address. VPN and Hyper-V virtual adapters are excluded.
- Make sure both devices are on the same Wi-Fi network (not a guest network).
- Turn off any VPN on the PC or the iPhone.
- Right-click the tray icon → **Allow through firewall**.

**Stuttering or delay.** Use 5 GHz Wi-Fi or connect the PC by Ethernet. Other devices streaming on the same network can add delay. Decoding and presentation run independently; overdue decoded frames are skipped to follow audio playback. With no audio, the newest decoded frame is shown immediately. The log includes the decoder format and periodic video timing statistics.

**No sound.** Check the default playback device in Windows sound settings. Apps that take exclusive control of the device can block AirScreen.

**Black window after connecting.** Stop mirroring and connect again. If it keeps happening, right-click the tray icon → **Open log file** and include the log in a [bug report](https://github.com/donerfolk/airscreen/issues).

Settings, pairing keys and the log live in `%APPDATA%\AirScreen`.

## Building from source

Requires Visual Studio 2022 with the *Desktop development with C++* workload.

```powershell
winget install Kitware.CMake
winget install ShiningLight.OpenSSL.Dev
pwsh -File scripts\build.ps1
```

`build.ps1` downloads FFmpeg into `third_party\prebuilt`, configures CMake and builds `build\Release\AirScreen.exe` with the required DLLs next to it. OpenSSL is found through `OPENSSL_ROOT_DIR`, the Shining Light install location, or vcpkg.

To build an installer, run [Inno Setup](https://jrsoftware.org/isinfo.php)'s `iscc` on `scripts\airscreen.iss` after a Release build. Pass `/DCrtDir=` pointing at the `x64\Microsoft.VC143.CRT` folder of your Visual Studio redistributables so the C++ runtime ships with the app.

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

## FAQ

**Is AirScreen free?** Yes, completely free and open source (GPL-3.0).

**Do I need an Apple TV?** No. AirScreen replaces an Apple TV for screen mirroring to your Windows PC.

**Do I need to install anything on my iPhone?** No. Use the built-in Screen Mirroring in Control Center. AirScreen shows up automatically on the same Wi-Fi network.

**Does it work with iPad or Mac?** iPad and Mac use the same AirPlay protocol and generally work, though they are untested.

**Does it work wirelessly?** Yes, entirely over Wi-Fi. Both devices must be on the same network (not a guest network).

## Credits

AirScreen's protocol support comes from [UxPlay](https://github.com/FDH2/UxPlay) and the projects it builds on (RPiPlay, shairplay, playfair). It also uses [FFmpeg](https://ffmpeg.org/), [OpenSSL](https://www.openssl.org/), [libplist](https://github.com/libimobiledevice/libplist) and llhttp.

AirScreen is not affiliated with Apple. AirPlay and iPhone are trademarks of Apple Inc.

## License

[GPL-3.0](LICENSE). Third-party licenses are listed in [NOTICE](NOTICE).
