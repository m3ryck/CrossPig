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
