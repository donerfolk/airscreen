---
name: AirScreen
description: Ambient Field idle window — a dusk-toned state field the iPhone's picture replaces, with the receiver name set huge on it.
colors:
  wait-top: "#1c3272"
  wait-deep: "#0b1536"
  wait-bloom: "#4a86d8"
  wait-violet: "#6d5bc4"
  wait-ink: "#13245a"
  pair-top: "#6e4418"
  pair-deep: "#2a180a"
  pair-bloom: "#d9974a"
  pair-rose: "#b86a5a"
  live-top: "#1a5040"
  live-deep: "#08201a"
  live-bloom: "#54b27e"
  fail-top: "#303036"
  fail-deep: "#151518"
  fail-ember: "#c05a50"
  fail-haze: "#4e4e56"
  label: "#ffffff"
  label-soft: "rgba(255, 255, 255, 0.84)"
  tile-rest: "rgba(255, 255, 255, 0.16)"
  tile-hover: "rgba(255, 255, 255, 0.27)"
  ghost-rim: "rgba(255, 255, 255, 0.47)"
  ghost-hover: "rgba(255, 255, 255, 0.14)"
typography:
  display:
    fontFamily: "Segoe UI Variable Display Semibold, Segoe UI Semibold, Segoe UI"
    fontSize: "clamp(44px, min(10.5vw, 20vh), 168px)"
    fontWeight: 600
    lineHeight: 1.33
    letterSpacing: "normal"
  pin:
    fontFamily: "Segoe UI Variable Display Semibold, Segoe UI Semibold, Segoe UI"
    fontSize: "clamp(64px, min(16vw, 30vh), 220px)"
    fontWeight: 600
    lineHeight: 1.33
    letterSpacing: "0.06em"
  state:
    fontFamily: "Segoe UI Variable Text Semibold, Segoe UI Semibold, Segoe UI"
    fontSize: "17px"
    fontWeight: 600
    lineHeight: 1.33
  route:
    fontFamily: "Segoe UI Variable Text, Segoe UI"
    fontSize: "15px"
    fontWeight: 400
    lineHeight: 1.33
  label:
    fontFamily: "Segoe UI Variable Text Semibold, Segoe UI Semibold, Segoe UI"
    fontSize: "13px"
    fontWeight: 600
    lineHeight: 1.33
rounded:
  tile: "12px"
spacing:
  margin: "clamp(32px, 6vw, 72px)"
  floor-gap: "48px"
  route-gap: "22px"
  tile-gap: "8px"
  action-split: "20px"
  tile-pad: "14px"
  tile-pad-compact: "10px"
  icon-gap: "8px"
  chevron-gap: "10px"
components:
  tile-off:
    backgroundColor: "{colors.tile-rest}"
    textColor: "{colors.label}"
    typography: "{typography.label}"
    rounded: "{rounded.tile}"
    padding: "0 14px"
    height: "40px"
  tile-off-hover:
    backgroundColor: "{colors.tile-hover}"
    textColor: "{colors.label}"
    typography: "{typography.label}"
    rounded: "{rounded.tile}"
    padding: "0 14px"
    height: "40px"
  tile-on:
    backgroundColor: "{colors.label}"
    textColor: "{colors.wait-ink}"
    typography: "{typography.label}"
    rounded: "{rounded.tile}"
    padding: "0 14px"
    height: "40px"
  action-ghost:
    backgroundColor: "transparent"
    textColor: "{colors.label}"
    typography: "{typography.label}"
    rounded: "{rounded.tile}"
    padding: "0 14px"
    height: "40px"
  action-ghost-hover:
    backgroundColor: "{colors.ghost-hover}"
    textColor: "{colors.label}"
    typography: "{typography.label}"
    rounded: "{rounded.tile}"
    padding: "0 14px"
    height: "40px"
  state-line:
    backgroundColor: "transparent"
    textColor: "{colors.label-soft}"
    typography: "{typography.state}"
  receiver-name:
    backgroundColor: "transparent"
    textColor: "{colors.label}"
    typography: "{typography.display}"
  pin-digits:
    backgroundColor: "transparent"
    textColor: "{colors.label}"
    typography: "{typography.pin}"
  route:
    backgroundColor: "transparent"
    textColor: "{colors.label-soft}"
    typography: "{typography.route}"
---

# Design System: AirScreen

## Overview

**Creative North Star: "The Ambient Field"**

The idle window is a screen waiting for a picture. There is no card, plate, or panel: the whole client area is a dusk-toned color field lit by two soft blooms, and the iPhone's mirrored video replaces it edge to edge when it arrives. The field is the status. Blue means waiting, amber means a pairing PIN is on screen, green means an iPhone has connected, graphite means the receiver could not start. The tones are muted on purpose: clearly colored, never electric, calm enough to sit next to a desk lamp all evening.

Type sits directly on the field in white, lifted by a soft shadow tinted from the field. A one-line state sentence holds the top-left corner. The receiver name iPhone will list anchors the bottom-left at display size, with the route to it underneath. Rename and three toggles line the floor as flat tiles. A PIN takes the name's place and the tiles leave.

Everything is painted by `ShellUi::paint` in `src/ui.cpp`: the field per pixel into a 32-bit DIB, with about one level of dither so long gradients never band; then the text shadow, blurred from a quarter-scale mask straight into the same pixels; then GDI+ for type, tiles, and stroke icons. Sizes are device-independent pixels at 96 DPI (`value × dpi / 96`). The default window is 1100×720 and the shell will not shrink below 720×480.

**Key Characteristics:**
- Full-bleed state field with no container; color carries connection state
- Muted dusk tones: saturated enough to name the state, never neon
- Two blooms drift over minutes while waiting or connected; pairing and failure hold still
- White type on the field lifted by a soft shadow in the field's own deep tone
- State sentence top-left; receiver name (or PIN) bottom-left at display size; route beneath the name
- Flat translucent tiles that flood solid white when on
- A dark mode toggle in the top-right corner dims every field; waiting becomes midnight blue

## Colors

A committed strategy in dusk tones: one muted but clearly colored field per state carries the entire surface, and white is the only ink on it. Every field is a 70° wash from its top tone to its deep tone, running top-left to bottom-right, plus two blooms that fall off as (1 − d²)² to nothing at their radius. Bloom strengths stay between roughly a third and two thirds so no glow ever burns.

### Primary
- **Dusk Blue** (`{colors.wait-top}` → `{colors.wait-deep}`): the default field, shown while the receiver waits or starts.
- **Sky Bloom** (`{colors.wait-bloom}`, 62%): upper-right bloom on the waiting field, radius 62% of the window width. Also the second bloom on the connected field at 36%.
- **Slate Violet Bloom** (`{colors.wait-violet}`, 50%): upper-left bloom on the waiting field, radius 46% of the width.
- **Waiting Ink** (`{colors.wait-ink}`): icon and label ink on a tile flooded white. Each field supplies its own ink from its deep tone (amber `#3e240e`, green `#0c3428`, graphite `#1e1e22`).

### Secondary
- **Pairing Amber** (`{colors.pair-top}` → `{colors.pair-deep}`): the field while a PIN is on screen, with `{colors.pair-bloom}` at 60% and `{colors.pair-rose}` at 45%.
- **Live Green** (`{colors.live-top}` → `{colors.live-deep}`): the field after an iPhone connects and before video arrives, with `{colors.live-bloom}` at 52%.

### Tertiary
- **Graphite Failure** (`{colors.fail-top}` → `{colors.fail-deep}`): the field when the receiver could not start, with a `{colors.fail-ember}` glow at 32% and `{colors.fail-haze}` at 50%.

### Neutral
- **Label White** (`{colors.label}`): receiver name, PIN, the state sentence outside waiting, tile labels and icons, the route's name segment, and the fill of an on tile.
- **Soft Light** (`{colors.label-soft}`): the waiting state sentence, route steps and chevrons, and the name's hover pencil. It is white at 84% so it takes the field's hue instead of reading gray.
- **Tile Rest / Tile Hover** (`{colors.tile-rest}` / `{colors.tile-hover}`): off toggle fills.
- **Ghost Rim / Ghost Hover** (`{colors.ghost-rim}` / `{colors.ghost-hover}`): the Rename action's outline and hover fill.

### Named Rules
**The Field-Is-Status Rule.** Connection state is shown by the field's color, never by a dot, pill, or badge. A new state gets a new field.

**The Dusk Rule.** Fields and blooms stay muted: no fully saturated system blue, orange, or green, and no bloom above two-thirds strength. A state must be recognizable, not glaring.

**The White-Ink Rule.** Nothing on the field is colored. Text, icons, and tiles are white at full, 84%, or translucent strength; the only dark ink sits on a tile flooded white.

**The Dark-Corner Rule.** Blooms live in the upper half. The bottom-left stays the darkest region of every field so white text holds contrast there.

### Dark mode
Dark mode keeps every field and changes only its brightness: each field's top and deep tones drop to 35%, and both blooms drop to 60% brightness at 45% of their strength. Waiting becomes a deep midnight blue (about `#091127` → `#030712`); pairing, connected, and failed become darker amber, green, and graphite, so the state still reads by hue. Type, tiles, inks, and the text shadow recipe are unchanged.

**The Midnight Rule.** Dark mode is one dimming applied to all four fields, never a separate palette and never a single color for every state.

## Typography

**Display Font:** Segoe UI Variable Display Semibold (fallback Segoe UI Semibold)
**Body Font:** Segoe UI Variable Text and Text Semibold (fallback Segoe UI and Segoe UI Semibold)

**Character:** The platform's own face, set the way ambient screens set the system face: very large, flush left, one weight step, no tracking tricks. Windows 10 has no Variable cuts, so Segoe UI Semibold carries the whole hierarchy there and the layout must read in both.

### Hierarchy
- **Display** (600, min(10.5% of width, 20% of height), clamped 44–168px): the receiver name. It shrinks to fit the line, never below 36px, then ends in an ellipsis.
- **PIN** (600, min(16% of width, 30% of height), clamped 64–220px, digits spaced 0.06em): pairing digits, placed one glyph at a time so they read back easily.
- **State** (600, 17px): the top-left state sentence.
- **Route** (400, 15px; the name segment 600): Control Center › Screen Mirroring › name.
- **Label** (600, 13px): tile labels.

### Named Rules
**The One-Family Rule.** Segoe UI only, at 400 and 600. No second family, no uppercase, no letterspaced labels.

**The Baseline Rule.** Every line starts on the same left margin, and baselines are placed from the font's cell ascent, not from layout boxes, so the lockup aligns to the pixel.

## Layout

The client area is the canvas; there is no inset container. The margin is 6% of the width, clamped to 32–72px, on every side.

- **Top-left:** the state sentence, its cap height sitting on the top margin.
- **Top-right:** the dark mode toggle, a 40px square tile on the right margin, centred on the state sentence's cap height. The state sentence stops 56px short of it. It hides along with the floor tiles.
- **Floor:** 40px tiles on the bottom margin, sized to their content and left-aligned. Rename comes first, then a 20px split, then Full Screen, Always on Top, and Require PIN 8px apart. When the row would cross the right margin, icons drop and side padding tightens from 14px to 10px.
- **Lockup:** stacks upward from 48px above the tiles, or from the bottom margin when the tiles are hidden. The route sits on that line; the name sits 22px above the route's cap height, with 22% of its own size reserved beneath for descenders.
- **Pairing:** tiles and route hide; the PIN digits sit on the bottom margin.
- **Full screen while idle:** tiles hide; the state sentence and lockup stay.

The live video surface (D3D11) fills the client while mirroring and is outside this system.

### Named Rules
**The Open-Top Rule.** The upper field stays empty apart from the state sentence. Content anchors low, like a lower third, so the window reads as a screen rather than a page.

## Elevation & Depth

Nearly flat. Depth comes from the field itself, the diagonal wash and its two blooms, plus one shadow: everything typeset straight on the field (state sentence, name or PIN, route and its chevrons) casts a soft drop shadow. The shadow is dropped 3px, blurred to roughly 12px (two box-blur passes each way on a quarter-scale mask, close to a Gaussian), and tinted with the field's deep tone at one third brightness, darkening the field by up to 55%. Thin lines get extra mask gain so small text still lifts; large type does not choke. Tiles and their labels cast nothing.

Motion is the drift of the bloom centers along slow sine paths (periods of 48–90 seconds, amplitude 7–8% of the window), repainted on a 100 ms timer. The field holds still during pairing, on failure, and whenever Windows client-area animation is turned off.

### Shadow Vocabulary
- **Field text** (offset 0 3px, blur ~12px, field deep tone ÷ 3 at up to 55%): under every piece of type set directly on the field, never under tiles.

### Named Rules
**The Lift-Not-Halo Rule.** The text shadow always has a downward offset and a soft blur, and always takes its color from the field. No black shadows, no zero-offset glows, no shadows on tiles.

**The No-Chrome Rule.** No gloss gradients, frosted panels, rims, or container outlines. When something needs separation, change its fill strength.

## Shapes

Tiles are rounded rectangles with a 12px radius at 40px tall: soft corners, not stadiums. Icons are 16px strokes at 1.6px with round caps and joins: a pencil, four corner brackets for Full Screen, a pushpin for Always on Top, a padlock for Require PIN, a crescent moon for dark mode. Route separators are drawn 7px chevrons at 1.5px, never a text glyph. The pencil that appears beside the name on hover scales with it (26% of the name size, 18–34px).

## Components

### Buttons
**Toggle tiles** (Full Screen, Always on Top, Require PIN): flat, light, and only as wide as their content.
- **Shape:** 12px radius, 40px tall, 14px side padding, 8px between icon and label
- **Off:** `{colors.tile-rest}` fill with white icon and label
- **Hover:** `{colors.tile-hover}` fill
- **On:** floods solid white; icon and label take the field's ink. Hover dims the fill to 91% white.

**Rename action:** a ghost tile, so an action never looks like a switch that is off.
- **Rest:** no fill, a 1.25px `{colors.ghost-rim}` outline, white icon and label
- **Hover:** `{colors.ghost-hover}` fill

**Dark mode toggle:** a 40px square tile in the top-right corner holding a stroked crescent moon, no label.
- **Off:** `{colors.tile-rest}` fill with a white moon; hover `{colors.tile-hover}`
- **On:** floods solid white with the moon in the field's ink; hover dims to 91% white
- **Behavior:** a click flips dark mode and saves it as `DarkMode` in `settings.ini`. Double-clicking any tile is two clicks; only a double-click on the open field toggles full screen.

### Receiver name
The object on the field. White display type at the bottom-left with the field-text shadow. It is a hit target with a hand cursor that opens the native rename dialog; on hover a Soft Light pencil appears after the last glyph. Use **AirScreen** as the example name in specs.

### Pairing PIN
When a PIN arrives the field turns amber and stops drifting, the state sentence reads "Enter this PIN on your iPhone", and the digits replace the name at PIN size. Tiles and route leave.

### State sentence
Top-left at 17px semibold. "Waiting for iPhone" is Soft Light; "Starting AirPlay…", "Connected — {device}", the PIN instruction, and failure text are Label White.

### Route
"Control Center › Screen Mirroring › {name}" under the name while waiting: steps in Soft Light, the name segment in white semibold, drawn chevrons between. Hidden once connected, failed, or pairing.

### Inputs / Fields
Rename uses the native Windows dialog (`IDD_RENAME`). Do not draw an inline text field on the field.

## Do's and Don'ts

### Do:
- **Do** let the field's color say the state: blue waiting, amber pairing, green connected, graphite failed.
- **Do** keep every field and bloom muted, dusk rather than daylight.
- **Do** apply dark mode as one dimming of all four fields, so waiting turns midnight blue and states still differ by hue.
- **Do** keep blooms in the upper half and text in the bottom-left and top-left.
- **Do** set the receiver name huge and flush left, shrinking it to fit before truncating.
- **Do** give type on the field its soft, offset, field-tinted shadow.
- **Do** flood an on toggle solid white so on and off read without color.
- **Do** hold the field still while pairing, on failure, and when Windows animations are off.
- **Do** draw icons and separators as strokes (1.6px icons, 1.5px chevrons).

### Don't:
- **Don't** put the idle UI in a card, plate, or panel, or bring back the frosted-glass look.
- **Don't** use fully saturated system colors or blooms above two-thirds strength.
- **Don't** add a status dot, pill, or badge; the field is the status.
- **Don't** put a label directly above the name or the PIN.
- **Don't** color text or icons on the field.
- **Don't** add black shadows, glows, sheens, rims, glass, or shadows under tiles.
- **Don't** draw fake macOS traffic lights or an Apple logo.
