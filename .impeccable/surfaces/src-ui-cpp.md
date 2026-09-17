---
version: 1
slug: "src-ui-cpp"
primary_target: "src/ui.cpp"
related_targets: ["src/main.cpp"]
---

# Idle window

Mode: Operate
Audience: someone at a Windows PC waiting for iPhone Screen Mirroring
Job: confirm the receiver name, pair with a PIN, start the mirror
Constraints: per-pixel DIB field and text shadow plus GDI+ idle / D3D11 video; 720×480–1080p; per-monitor DPI; Windows 10 without Segoe UI Variable

Direction: Ambient Field (seed aba659fb), code-led, chosen 2026-09-13 to replace the Continuity Card glass plate. No comp. Same day, user asked for less harsh color and shadows behind the text: fields moved to muted dusk tones and field text gained a soft tinted drop shadow.

Memorable moment: a dusk-blue field with the receiver name huge and white at the bottom-left, lifted by a soft shadow; a PIN turns the whole window amber and stills it.

## Inventory

| Region | Medium |
| --- | --- |
| State field (blue / amber / green / graphite) with two blooms | Per-pixel fill into a 32-bit DIB |
| Text shadow under all field type | Quarter-scale GDI+ mask, box-blurred, blended into the DIB |
| State sentence, top-left | GDI+ text |
| Dark mode toggle, top-right (saved as DarkMode) | GDI+ square tile + stroked crescent |
| Receiver name, bottom-left, clickable | GDI+ text + hover pencil |
| Route: Control Center › Screen Mirroring › name | GDI+ text + drawn chevrons |
| Rename ghost tile + three toggle tiles | GDI+ rounded rects + stroke icons |
| PIN digits in place of the name | GDI+ text |
| Live video | Existing D3D11 (unchanged) |

Signature interaction: toggles flood white in place; a PIN recolors and stills the whole field.

## Open

- The field cuts between states; a short crossfade is the next motion step.
- The live-overlay gear in src/main.cpp keeps the old graphite circle, rim, and a Segoe MDL2 font glyph.
- The failure sentence points to "Settings", which the idle window does not show (right-click menu holds the log).
