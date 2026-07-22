# Custom Themes

Custom UI themes are installed on the SD card. Create a directory at:

```text
/themes/<id>/theme.json
```

For example, copy `examples/themes/paper` to `/themes/paper`. Then open
`Settings > Display > UI Theme > Custom`. The editor lets you choose a source
theme for Home, Header, List, Menu, Popup, Input, Hints, and Status Bar.

## Manifest v1

`schemaVersion` must be `1`. `id` must match its directory name and use only
lowercase letters, digits, and hyphens (maximum 32 characters). `name` is shown
in the selector (maximum 48 characters). A manifest is limited to 4 KB.

`base` and component values may be one of:

- `classic`
- `lyra`
- `lyra-extended`
- `lyra-carousel`
- `minimal`
- `dashboard`
- `roundedraff`

`base` supplies the Home layout and fallback renderer. Component fields are
optional and may choose a different existing renderer for `header`, `list`,
`menu`, `popup`, `input`, `hints`, and `status`. This deliberately reuses
firmware-tested interaction and layout instead of loading executable code from
the SD card.

At most 16 valid packages are listed as editor sources. Invalid, missing, or
unsupported manifests are skipped with a `THEME` log entry. Selecting a package
copies its resolved built-in component into the Custom theme; the saved choice
continues to work if the package is later removed.

## Authoring limits

The first format covers renderer composition only. It does not execute scripts,
change reader document rendering, or alter sleep screens. Those restrictions
keep custom packages safe on the ESP32-C3 and avoid a second framebuffer or
per-frame heap allocation.

## Declarative themes (v2)

A fully independent theme uses `schemaVersion: 2` and `engine: "declarative"`.
It is selected directly from `Settings > Display > UI Theme`; it does not name
or inherit from a built-in theme. The current v2 renderer customizes Home and
its action menu with a safe, data-only layout.

```text
/themes/monochrome/theme.json
/themes/monochrome/assets/home.bmp
```

`home.cover` uses integer coordinates from `0` to `1000`, relative to the
Home cover area. `menu.columns`, `rowHeight`, and `gap` define a list or
grid for firmware-owned actions. See `examples/themes/monochrome/theme.json`.

All component objects are optional and fall back to Lyra-compatible values:

- `home`: top padding, cover-area height, menu offset, background, and cover geometry.
- `header`: header height, screen spacing, side padding, and tab dimensions.
- `list`: normal and subtitle row heights.
- `menu`: columns, row height, and gap.
- `popup`: position, margins, frame, corners, progress bar, and text treatment.
- `input`: keyboard dimensions, key treatment, and text-field strokes.
- `hints`: bottom-hint height and side-hint width.
- `status`: reader margins and progress-bar height.

These properties change renderer metrics only; firmware-owned labels, input
behavior, battery information, and reading data remain under firmware control.

Assets must live under `assets/`, use safe relative names, and be 1-bit BMPs.
The firmware opens one asset at a time and draws it into the existing display
buffer. Invalid packages are omitted from the selector; if an active package
is removed, the device returns to Lyra.

## Structural Settings layouts (v3)

Schema v3 keeps every v2 component and adds a bounded, declarative layout for
the Settings screen. It can change the menu structure without replacing the
firmware-owned actions or executing theme code:

```json
{
  "schemaVersion": 3,
  "engine": "declarative",
  "screens": {
    "settings": {
      "layout": "grid",
      "columns": 2,
      "gap": 8,
      "cardRadius": 8,
      "cardHeight": 64,
      "order": ["uiTheme", "sleepScreen"]
    }
  }
}
```

`layout` accepts `list`, `cards`, or `grid`. Grid layouts support one to three
columns and spatial button navigation. `order` accepts up to eight stable keys
from `SettingInfo.key`; matching items move to the front and every other
firmware setting remains visible afterward. Card and grid layouts omit section
heading rows but keep all interactive items and submenus.

The firmware retains only a fixed summary and 32-bit hashes of the requested
keys. It never retains the JSON document during rendering. A Settings layout
is limited to 48 resolved entries; larger or invalid screen definitions fall
back to the normal list without disabling the rest of the theme.

## Modular Home layouts (v3)

The v3 `home` object can compose reading-focused modules instead of placing one
fixed cover over a decorative background:

```json
{
  "home": {
    "layout": "shelf",
    "recentBooks": 3,
    "bookGap": 10,
    "showCover": true,
    "showTitle": true,
    "showAuthor": false,
    "showProgress": true,
    "showBookStats": false,
    "showGlobalStats": true
  }
}
```

Available compositions are:

- `shelf`: displays up to three recent books simultaneously.
- `spotlight`: displays one selected book using the normalized `cover`
  geometry; other recent books remain navigable.
- `dashboard`: displays the selected cover and book information side by side.

When `recentBooks` is greater than one, Left and Right navigate books, Up and
Down move between the book composition and the action menu, and Confirm opens
the selected book. The optional book-statistics module displays reading time,
progress, and pages turned. The global module displays total reading time,
completed books, and sessions. These values come from firmware-owned reading
data and cannot be supplied or modified by a theme.

`background` remains supported for compatibility with existing v2 packages,
but Theme Studio no longer exports decorative Home images. Disabling
`showCover` also skips thumbnail generation, which avoids unnecessary SD and
EPUB work for text-and-statistics-only designs.

## Visual Home canvas (v4)

Schema v4 adds a bounded canvas to the declarative Home. Themes position up to
eight firmware-owned blocks with normalized `0..1000` frames. Blocks cannot
execute code or provide their own reading data.

```json
{
  "schemaVersion": 4,
  "engine": "declarative",
  "home": {
    "layoutEngine": "canvas",
    "blocks": [
      {
        "type": "recentBooks",
        "variant": "cards",
        "frame": { "x": 30, "y": 20, "width": 940, "height": 520 }
      }
    ],
    "actions": {
      "presentation": "panel",
      "pinned": ["browse"],
      "order": ["browse", "recents", "stats", "transfer", "settings"],
      "panel": { "columns": 2 }
    }
  }
}
```

Supported blocks are `recentBooks`, `bookProgress`, `bookStats`,
`globalStats`, `quickActions`, and `menuTrigger`. Each type may appear once.
Frames must remain completely inside the canvas. Every block is optional, and
an empty `blocks` array creates a blank Home below the firmware header. A
`recentBooks` block accepts `variant: "cards"` (the default) or `"plain"` to
remove the individual book frames.

Action presentation accepts:

- `inline`: an optional `quickActions` block displays every available action.
- `panel`: an optional `menuTrigger` block opens the action panel.
- `hybrid`: `quickActions` displays up to three pinned actions and
  `menuTrigger` opens the complete panel when those optional blocks exist.

The action order is completed with any omitted firmware actions, so a theme
cannot permanently hide Settings or library access. OPDS, reading statistics,
and saved items appear only when their underlying firmware feature has data.
On the Home, the logical Back control—the first button from the left with the
default X4 mapping—is always labeled Menu and opens the complete action panel.
This keeps navigation reachable even with an empty canvas. Up and Down move
between canvas blocks; Left and Right navigate within recent books or quick
actions. The panel uses spatial navigation and Back returns to the canvas.

The registry stores fixed block/action arrays rather than retaining the JSON
tree. Rendering uses the existing framebuffer and the panel is a Home state,
not a separate Activity.

## Importing a package

Open the device's Wi-Fi portal and select **Themes**. The portal imports a
`.cptheme` file (a ZIP archive) in the browser, then transfers only its
validated contents to the SD card. A package may contain `theme.json` and up
to 16 flat `assets/*.bmp` files, with at most 2 MB after extraction. Replacing
an installed package requires confirmation. The device validates the manifest,
file layout, size, and BMP format again before publishing the package, so a
manual copy to `/themes/<id>/` remains supported.

The current importer accepts declarative v2, v3, and v4 packages. Font files and
scripts are intentionally not accepted.

## Theme Studio

The standalone [Theme Studio](../theme-studio/README.md) application creates
v4 packages in a normal web browser, including the visual Home canvas and
Settings list, card, and grid layouts. It is intentionally separate from the
firmware and can be deployed to any static-file host. The device portal is
only responsible for importing and managing the resulting package.
