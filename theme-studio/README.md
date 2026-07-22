# CrossInk Theme Studio

A standalone browser application for creating declarative CrossInk v4
`.cptheme` packages. It runs entirely in the browser: no theme image or
metadata is uploaded to a server.

The interface is built with React and local shadcn-style components backed by
Radix primitives. Component controls combine accessible sliders, editable
numeric values, switches, accordions, and tabbed live previews.

The task-focused workflow separates theme setup, Home composition, shared
components, and Settings structure. Drafts are saved locally in the browser,
and an existing `.cptheme` package or `theme.json` manifest can be reopened for
editing. Home blocks can be automatically arranged into a safe starting layout
before fine tuning on the canvas.

The Home editor has normalized X4 (480×800) and X3 (528×792) previews with
draggable and resizable reading blocks. Blocks can be added, removed directly
from the canvas, or cleared completely; recent books can use framed cards or an
unframed composition. Design mode edits geometry, while Test mode exercises the
Menu panel from the first device button or the keyboard. Actions can remain
inline, move entirely into the panel, or combine pinned Home shortcuts with the
panel.

## Development

```bash
npm install
npm run dev
```

Node.js 20.19 or newer is required by the current Vite toolchain.

Create a production build with `npm run build`. The generated `dist/` directory
can be deployed to GitHub Pages, Netlify, Vercel, or any static-file host.

## Output

The Studio produces a ZIP archive with a `.cptheme` extension containing a v4
`theme.json`. Import that file using the **Themes** page in the CrossInk Wi-Fi
portal. Legacy themes may still contain a monochrome Home background, but the
Studio focuses on useful reading modules rather than images hidden by covers.
