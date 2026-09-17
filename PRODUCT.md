# Product

<!-- impeccable:product-schema 1 -->

## Platform

windows-desktop

## Stack

Native Win32 + D3D11 (existing codebase). Not web.

## Users

[Inferred from the approved AirScreen plan] Someone at a Windows PC who wants iPhone Screen Mirroring on the same Wi-Fi, without buying an Apple TV.

## Product Purpose

AirScreen makes the PC appear in iOS Control Center as an AirPlay receiver and shows the mirrored picture and sound in a window.

Success: iPhone lists **AirScreen**, tap connects, video and audio play with low PC-side delay.

## Positioning

Windows AirPlay Screen Mirroring receiver with an original ambient-display UI and a custom decode/present path (no GStreamer). Protocol from UxPlay (GPLv3).

## Operating Context

Used in a dim room or at a desk, often as a second display. Waiting is the default state. Pairing PIN appears on first connect (iOS 16+). Video then owns the window. Close hides to the tray.

## Capabilities and Constraints

- Advertise as AirPlay receiver; rename; optional PIN; firewall helper; fullscreen; always-on-top
- Native Win32 shell (not Electron). Idle UI is a per-pixel field plus GDI+; mirrored video is D3D11
- Must stay readable at 720×480 through 1080p, per-monitor DPI, and on Windows 10 without Segoe UI Variable
- [Assumption] No brand guidelines beyond the name AirScreen

## Brand Commitments

Name: AirScreen. Voice: direct, physical, no marketing claims.
Visual world: an ambient screen waiting for a picture — a full-bleed state field, the receiver name in large white system type, flat controls. Never fake macOS chrome; never a card-in-a-window settings panel or frosted-glass plate (retired 2026-09-13 as outdated).

## Evidence on Hand

No photography, logo files, or testimonials. Do not invent reviews or latency numbers in the UI.

## Product Principles

- The window is the screen; waiting should feel like a display with no signal, not a settings page
- The receiver name iOS will show is the primary object
- PIN, when present, outranks everything else
- Controls the user needs while waiting live in the window, not only the tray
