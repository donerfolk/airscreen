# AirScreen

Free Windows AirPlay receiver. Your iPhone sees the PC in **Control Center → Screen Mirroring**, same idea as LonelyScreen. Original app (GPLv3) using the open-source [UxPlay](https://github.com/FDH2/UxPlay) protocol stack with a low-latency D3D11 / WASAPI renderer.

## Use

1. PC and iPhone on the same Wi-Fi (not guest/AP-isolation).
2. Run `AirScreen.exe`. Allow the firewall if Windows asks.
3. iPhone: Control Center → Screen Mirroring → **AirScreen**.
4. Tray icon: rename, always-on-top, pairing PIN, fullscreen.

Typical latency on 5 GHz Wi-Fi is about 80–150 ms (iPhone encode + network). AirScreen keeps the PC side to hardware decode and 1-frame present.

## Build (Windows 10/11 x64)

Needs Visual Studio 2022 with “Desktop development with C++”, CMake, OpenSSL, FFmpeg.

```powershell
winget install Kitware.CMake
winget install ShiningLight.OpenSSL.Dev
powershell -ExecutionPolicy Bypass -File scripts\fetch-deps.ps1

cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

`AirScreen.exe` lands in `build\Release\` with FFmpeg/OpenSSL DLLs copied beside it.

Installer: compile `scripts\airscreen.iss` with [Inno Setup](https://jrsoftware.org/isinfo.php) after a Release build.

## Troubleshooting

- Receiver missing on iPhone: same subnet, disable VPN, allow AirScreen in Windows Firewall (tray → Allow through firewall).
- iOS asks for a PIN: tray → Require pairing PIN, or type the on-screen code.
- No audio: default Windows playback device, not exclusive-mode apps hogging WASAPI.

## License

GPLv3. See [LICENSE](LICENSE) and [NOTICE](NOTICE).
