# CrossInk Theme Studio

A standalone browser application for creating declarative CrossInk v2
`.cptheme` packages. It runs entirely in the browser: no theme image or
metadata is uploaded to a server.

The interface is built with React and local shadcn-style components backed by
Radix primitives. Component controls combine accessible sliders, editable
numeric values, switches, accordions, and tabbed live previews.

## Development

```bash
npm install
npm run dev
```

Node.js 20.19 or newer is required by the current Vite toolchain.

Create a production build with `npm run build`. The generated `dist/` directory
can be deployed to GitHub Pages, Netlify, Vercel, or any static-file host.

## Output

The Studio produces a ZIP archive with a `.cptheme` extension. It contains a
v2 `theme.json` and, if selected, `assets/background.bmp` as an 800×480 1-bit
BMP. Import that file using the **Themes** page in the CrossInk Wi-Fi portal.
