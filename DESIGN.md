---
name: AirScreen
description: Continuity plate idle shell — full-bleed glass, the receiver name is the object.
colors:
  field: "#1d1d1f"
  plate: "#2c2c2e"
  plate-hi: "#3a3a3c"
  capsule-rest: "#303032"
  rim: "#606064"
  hairline: "#48484a"
  label: "#f5f5f7"
  secondary: "#a1a1a6"
  on-ink: "#ffffff"
  blue: "#0a84ff"
  wait: "#8e8e93"
  live: "#30d158"
  pair: "#ff9f0a"
  fail: "#ff453a"
typography:
  display:
    fontFamily: "SF Pro Display Medium, SF Pro Display Semibold, SF Pro Display, Segoe UI Variable Display Semibold, Segoe UI Variable Display, Segoe UI"
    fontSize: "clamp(36px, 8.2vw, 72px)"
    fontWeight: 600
    lineHeight: 1.35
    letterSpacing: "-0.08em"
  pin:
    fontFamily: "SF Pro Display, Segoe UI Variable Display, Segoe UI"
    fontSize: "clamp(56px, 16vw, 96px)"
    fontWeight: 400
    lineHeight: 1.2
    letterSpacing: "normal"
  title:
    fontFamily: "SF Pro Text, SF Pro, Segoe UI Variable Text, Segoe UI Variable, Segoe UI"
    fontSize: "13px"
    fontWeight: 400
    lineHeight: 1.3
    letterSpacing: "normal"
  body:
    fontFamily: "SF Pro Text, SF Pro, Segoe UI Variable Text, Segoe UI Variable, Segoe UI"
    fontSize: "17px"
    fontWeight: 400
    lineHeight: 1.3
    letterSpacing: "normal"
  label:
    fontFamily: "SF Pro Text, SF Pro, Segoe UI Variable Text, Segoe UI Variable, Segoe UI"
    fontSize: "12px"
    fontWeight: 400
    lineHeight: 1.2
    letterSpacing: "normal"
rounded:
  plate: "28px"
  stadium: "999px"
  gear: "999px"
spacing:
  field: "24px"
  plate-top: "22px"
  pad: "36px"
  gap: "10px"
  inset: "12px"
  tight: "6px"
components:
  plate:
    backgroundColor: "{colors.plate}"
    textColor: "{colors.label}"
    rounded: "{rounded.plate}"
    padding: "{spacing.pad}"
  pill:
    backgroundColor: "{colors.plate-hi}"
    textColor: "{colors.label}"
    typography: "{typography.label}"
    rounded: "{rounded.stadium}"
    width: "108px"
    height: "28px"
  capsule-off:
    backgroundColor: "{colors.capsule-rest}"
    textColor: "{colors.secondary}"
    typography: "{typography.label}"
    rounded: "{rounded.stadium}"
    padding: "0 10px 0 12px"
    height: "44px"
  capsule-off-hover:
    backgroundColor: "{colors.plate-hi}"
    textColor: "{colors.label}"
    typography: "{typography.label}"
    rounded: "{rounded.stadium}"
    padding: "0 10px 0 12px"
    height: "44px"
  capsule-on:
    backgroundColor: "{colors.blue}"
    textColor: "{colors.on-ink}"
    typography: "{typography.label}"
    rounded: "{rounded.stadium}"
    padding: "0 10px 0 12px"
    height: "44px"
  receiver-name:
    backgroundColor: "transparent"
    textColor: "{colors.label}"
    typography: "{typography.display}"
  receiver-name-hover:
    backgroundColor: "transparent"
    textColor: "{colors.on-ink}"
    typography: "{typography.display}"
  pin-digits:
    backgroundColor: "transparent"
    textColor: "{colors.label}"
    typography: "{typography.pin}"
  wordmark:
    backgroundColor: "transparent"
    textColor: "{colors.secondary}"
    typography: "{typography.title}"
  gear:
    backgroundColor: "{colors.plate}"
    textColor: "{colors.label}"
    rounded: "{rounded.gear}"
    size: "40px"
  gear-hover:
    backgroundColor: "{colors.plate-hi}"
    textColor: "{colors.label}"
    rounded: "{rounded.gear}"
    size: "40px"
---

# Design System: AirScreen

## Overview

**Creative North Star: "The Continuity Plate"**

The idle window is a dim display with no signal: a near-black field and one frosted glass plate. The object on that plate is the receiver name iPhone will list — not a dashboard, not a settings page. Status is a small pill and one muted line. Controls sit on the floor as outlined capsules. When a pairing PIN appears, it takes the plate; the name and the capsules leave.

This is Apple Continuity language on a native Win32 client, not a macOS window replica. Type is SF Pro when the machine has it, otherwise Segoe UI Variable. Display faces render medium or semibold (bold only if those cuts are missing). The plate is opaque graphite with a white sheen and a hairline rim — GDI+ cannot photograph blur, and the system does not pretend it can.

Sizes are device-independent pixels at 96 DPI (`MulDiv(value, dpi, 96)`). The default window is 1100×720; the shell will not shrink below 720×480.

**Key Characteristics:**
- Full-bleed glass plate on a darker field, 24px margin
- Receiver name as the only large object (tight 0.92 tracking)
- Outlined stadium capsules; Continuity Blue only when a control is on
- Status in a top-right pill plus one muted line under the name
- PIN replaces the name at the same optical center; capsules hide

## Colors

Graphite Continuity palette: two stacked darks, one blue that appears only as an on-state, and four status lights that live in the pill dot.

### Primary
- **Continuity Blue** (`{colors.blue}`): Fill for a floor capsule whose setting is on (Full Screen, Always on Top, Require PIN). Never used on the field, the plate, the idle pill, or the wordmark.

### Tertiary
- **Wait Gray** (`{colors.wait}`): Idle pill dot.
- **Live Green** (`{colors.live}`): Connected pill dot.
- **Pairing Amber** (`{colors.pair}`): PIN-on-plate pill dot.
- **Fail Red** (`{colors.fail}`): Start-failed pill dot.

### Neutral
- **Graphite Field** (`{colors.field}`): Window client fill around the plate.
- **Frosted Plate** (`{colors.plate}`): The glass card, the app-mark body, and the live-overlay gear at rest.
- **Plate Lift** (`{colors.plate-hi}`): Waiting pill fill, capsule hover fill, gear hover fill.
- **Capsule Rest** (`{colors.capsule-rest}`): Off-state capsule fill (slightly darker than the plate so the outline reads).
- **Glass Rim** (`{colors.rim}`): 1.15px plate stroke.
- **Hairline** (`{colors.hairline}`): 1px capsule outline when off.
- **Label** (`{colors.label}`): Name, PIN, pill text, on-state / hover ink.
- **Secondary Label** (`{colors.secondary}`): Wordmark, status line, how-to line, off-state capsule ink.
- **On Ink** (`{colors.on-ink}`): Name hover, and ink on a blue capsule.

### Named Rules
**The Blue-When-On Rule.** Continuity Blue fills a control only while that control is on. Idle chrome stays graphite.

**The Pill-Dot Rule.** Wait, pair, live, and fail exist only as the status-pill dot (plus its soft pulse glow). They do not color the plate, the name, or the capsules.

## Typography

**Display Font:** SF Pro Display Medium / Semibold (fallback Segoe UI Variable Display Semibold, then Segoe UI)
**Body Font:** SF Pro Text (fallback Segoe UI Variable Text, then Segoe UI)

**Character:** One San Francisco family on Windows. Quiet UI text; the name is the only display line, tracked tight so it sits as a single object.

Probe order in the shell: display medium/semibold cuts first; if the face name does not contain “Medium” or “Semibold”, the name uses bold on the regular display cut. PIN uses the regular display face, not the medium cut, and does not apply tracking.

### Hierarchy
- **Display** (600, clamp 36–72px from inner width / 8.2, line-height 1.35, tracking 0.92 / `-0.08em`): Receiver name, centered, slightly above geometric middle.
- **PIN** (400, max(56px, inner width / 6.2)): Pairing digits; owns the plate; no tracking.
- **Title** (400, 13px): Wordmark “AirScreen” top-left; how-to line under status.
- **Body** (400, 17px): Status under the name (“Waiting for iPhone”) and the PIN hint (“Enter this PIN on your iPhone”).
- **Label** (400, 12px): Status-pill text and capsule labels.

### Named Rules
**The Tight-Name Rule.** Only the receiver name is tracked (glyph advance × 0.92) and cut medium/semibold. PIN, status, and controls stay untracked regular text.

**The Two-Face Rule.** Display cuts for the name and PIN; Text cuts for everything else. Do not mix in a third family.

## Layout

Client area is the field. The plate is inset `{spacing.field}` (24px) on all sides. If that plate would be narrower than 200px it becomes 92% of the window; if shorter than 160px it becomes 88% of the height. Inner content inset is `{spacing.pad}` (36px) left/right/bottom and `{spacing.plate-top}` (22px) at the top.

Header row: wordmark left, status pill right (108×28px), sharing a 28px-tall band. The receiver name sits at 42% of the plate height. Status follows 4px under the name; the how-to line 6px under that, and lifts to 30px above the capsules if it would collide. Capsules are a 44px-tall floor row, 10px gaps, equal width, minimum 96px each, hidden while a PIN is showing or the window is fullscreen.

The live video surface is D3D11 and fills the client when connected; this document does not specify that picture. The 40px circular gear is live-overlay chrome only (12px from the top-right of the client), hidden on the idle plate.

### Named Rules
**The Optical-Center Rule.** The name is the object at ~42% of the plate. A PIN replaces it at ~28% of the plate. Nothing else is allowed to become large.

## Elevation & Depth

Depth is tonal stacking plus a soft umbra and a painted sheen — not drop-shadow chrome and not backdrop blur. The field is a flat fill. The plate sits on eight stacked black rounds (alpha 7–14, shifted down and slightly expanded). Inside the plate, a vertical white gradient (alpha 56 → 0) covers the top 58% of the height, with a brighter 18px top-edge wash (alpha 70 → 0). A 1.15px `{colors.rim}` stroke seals the glass. The pill dot pulses: fill alpha 140–255 and a 3px halo alpha 30–70, 40ms timer, sine over ~430ms.

### Shadow Vocabulary
- **Plate umbra** (eight GDI+ rounds, black alpha 7–14, y += i×1.1): Under the plate only.
- **Dot glow** (ellipse, status color, pulse alpha): Around the pill dot only.

### Named Rules
**The Sheen-Not-Blur Rule.** Frost is a white gradient on opaque plate color. Do not introduce backdrop-filter, glass photography, or extra shadow layers on capsules.

## Shapes

The plate is a 28px-radius round rect — the only large corner. Every control is a stadium: radius = half its height (pill 14px, capsules 22px). The live gear is a circle (window region = size). Capsule icons are 14px stroked GDI+ marks (round caps and joins, stroke ≈ 1.4px): pencil, display, pin, lock. The app mark is a rounded display body in plate color with a pale glass screen — not a logo lockup.

### Named Rules
**The Outlined-Capsule Rule.** Off = rest fill + 1px hairline. On = solid blue, no outline. Icons stay strokes, not fills and not font glyphs.

## Components

### Buttons
Floor capsules are the only idle buttons.

- **Shape:** Stadium (radius 22px), height 44px, min width 96px
- **Off:** `{colors.capsule-rest}` fill, `{colors.hairline}` 1px stroke, `{colors.secondary}` ink
- **Hover:** `{colors.plate-hi}` fill, same stroke, `{colors.label}` ink
- **On:** `{colors.blue}` fill, no stroke, `{colors.on-ink}` ink
- **Content:** 14px stroke icon, 6px gap, 12px label. Labels: Rename, Full Screen, Always on Top, Require PIN. Rename never takes the on fill.

### Chips
- **Style:** Status pill, 108×28px stadium, `{colors.plate-hi}` fill, no stroke
- **State:** Dot + label. Waiting / Pairing / Live / Failed. Dot color from the tertiary set; label always `{colors.label}`

### Cards / Containers
- **Corner Style:** 28px
- **Background:** `{colors.plate}` with inner sheen
- **Shadow Strategy:** Plate umbra only
- **Border:** 1.15px `{colors.rim}`
- **Internal Padding:** 36px (22px top)

### Inputs / Fields
Rename uses a native Windows dialog, not plate chrome. The receiver name is a hit target (hand cursor, brightens to on-ink) that opens that dialog. Do not draw an inline text field on the plate.

### Navigation
Not a nav bar. Wordmark “AirScreen” in secondary 13px at the top-left of the plate; status pill at the top-right. Native Win32 caption remains OS chrome and is outside this system.

### Receiver name
The product. Centered, tracked, display medium/semibold. Example copy for specs: **AirScreen**. The visible string is the user’s receiver name, never canned marketing. How-to line: `On iPhone: Control Center  →  Screen Mirroring  →  {name}`.

### Pairing PIN
When present, the name, how-to, and capsules hide. Digits are regular display type, centered, with the 17px hint beneath. Pill switches to Pairing / amber.

### App mark
16px / 32px GDI+ icon: rounded display body in plate color, pale glass screen. Used for the window and tray. Not a wordmark replacement.

### Gear (live overlay)
40px circle, `{colors.plate}` at rest, `{colors.plate-hi}` on hover, `{colors.label}` glyph. Idle waiting does not show it. Video owns the window when live.

## Do's and Don'ts

### Do:
- **Do** keep the plate full-bleed with a 24px field margin and a 28px corner.
- **Do** treat the receiver name as the only large object; use AirScreen as the spec example name.
- **Do** draw floor capsules outlined at rest and Continuity Blue only when on.
- **Do** let a PIN replace the name and hide the capsules.
- **Do** use stroked GDI+ marks (pencil, display, pin, lock) at 14px for capsule icons.

### Don't:
- **Don't** draw fake macOS traffic lights or an Apple logo.
- **Don't** put Continuity Blue on idle or waiting chrome.
- **Don't** introduce a third type family, kickers, or uppercase eyebrows.
- **Don't** fill the plate with settings; waiting is a display with no signal.
- **Don't** box PIN digits or add extra cards — one plate, one object.
