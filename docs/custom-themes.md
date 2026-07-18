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

## Importing a package

Open the device's Wi-Fi portal and select **Themes**. The portal imports a
`.cptheme` file (a ZIP archive) in the browser, then transfers only its
validated contents to the SD card. A package may contain `theme.json` and up
to 16 flat `assets/*.bmp` files, with at most 2 MB after extraction. Replacing
an installed package requires confirmation. The device validates the manifest,
file layout, size, and BMP format again before publishing the package, so a
manual copy to `/themes/<id>/` remains supported.

The current importer accepts declarative v2 packages only. Font files and
scripts are intentionally not accepted.

## Theme Studio

The standalone [Theme Studio](../theme-studio/README.md) application creates
v2 packages in a normal web browser. It is intentionally separate from the
firmware and can be deployed to any static-file host. The device portal is
only responsible for importing and managing the resulting package.
