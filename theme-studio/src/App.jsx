import { useMemo, useState } from "react";
import JSZip from "jszip";
import {
  Download,
  ImagePlus,
  MonitorSmartphone,
  RotateCcw,
  Sparkles,
} from "lucide-react";
import {
  Accordion,
  AccordionContent,
  AccordionItem,
  AccordionTrigger,
} from "./components/ui/accordion";
import { Badge } from "./components/ui/badge";
import { Button } from "./components/ui/button";
import {
  Card,
  CardContent,
  CardDescription,
  CardHeader,
  CardTitle,
} from "./components/ui/card";
import { Input } from "./components/ui/input";
import { Label } from "./components/ui/label";
import { Slider } from "./components/ui/slider";
import { Switch } from "./components/ui/switch";
import { Tabs, TabsContent, TabsList, TabsTrigger } from "./components/ui/tabs";

const defaults = {
  name: "My Theme",
  id: "my-theme",
  homeTopPadding: 56,
  homeCoverAreaHeight: 242,
  homeMenuTopOffset: 16,
  coverX: 200,
  coverY: 20,
  coverWidth: 600,
  coverHeight: 820,
  cornerRadius: 10,
  columns: 2,
  rowHeight: 54,
  gap: 10,
  headerHeight: 84,
  topPadding: 5,
  verticalSpacing: 16,
  sidePadding: 20,
  tabHeight: 40,
  tabSpacing: 8,
  listRowHeight: 36,
  subtitleRowHeight: 60,
  popupTop: 165,
  popupMarginX: 16,
  popupMarginY: 12,
  popupRadius: 6,
  popupFrame: 2,
  popupProgress: 4,
  popupBold: false,
  popupInverted: false,
  keyWidth: 31,
  keyHeight: 40,
  keySpacing: 0,
  keyRadius: 6,
  keyboardWidth: 90,
  fieldPadding: 6,
  fieldThickness: 1,
  cursorThickness: 3,
  fillKeys: false,
  outlineKeys: false,
  hintsHeight: 40,
  sideHintsWidth: 30,
  statusMarginX: 5,
  statusMarginY: 19,
  progressHeight: 16,
};

const groups = [
  {
    value: "home",
    title: "Home and cover",
    fields: [
      ["homeTopPadding", "Top padding", 0, 200],
      ["homeCoverAreaHeight", "Cover area", 100, 600],
      ["homeMenuTopOffset", "Menu offset", 0, 100],
      ["coverX", "Cover X", 0, 1000],
      ["coverY", "Cover Y", 0, 1000],
      ["coverWidth", "Cover width", 1, 1000],
      ["coverHeight", "Cover height", 1, 1000],
      ["cornerRadius", "Cover radius", 0, 100],
    ],
  },
  {
    value: "menu",
    title: "Action menu",
    fields: [
      ["columns", "Columns", 1, 4],
      ["rowHeight", "Row height", 0, 300],
      ["gap", "Gap", 0, 100],
    ],
  },
  {
    value: "header",
    title: "Header and tabs",
    fields: [
      ["headerHeight", "Header height", 24, 160],
      ["topPadding", "Top padding", 0, 80],
      ["verticalSpacing", "Vertical spacing", 0, 80],
      ["sidePadding", "Side padding", 0, 120],
      ["tabHeight", "Tab height", 20, 120],
      ["tabSpacing", "Tab spacing", 0, 80],
    ],
  },
  {
    value: "list",
    title: "Lists",
    fields: [
      ["listRowHeight", "Row height", 20, 120],
      ["subtitleRowHeight", "Subtitle row", 30, 160],
    ],
  },
  {
    value: "popup",
    title: "Popups",
    fields: [
      ["popupTop", "Top position", 0, 800],
      ["popupMarginX", "Horizontal margin", 4, 80],
      ["popupMarginY", "Vertical margin", 4, 80],
      ["popupRadius", "Corner radius", 0, 80],
      ["popupFrame", "Frame thickness", 1, 6],
      ["popupProgress", "Progress height", 1, 20],
    ],
    toggles: [
      ["popupBold", "Bold text"],
      ["popupInverted", "Inverted surface"],
    ],
  },
  {
    value: "input",
    title: "Input and keyboard",
    fields: [
      ["keyWidth", "Key width", 16, 80],
      ["keyHeight", "Key height", 20, 80],
      ["keySpacing", "Key spacing", 0, 24],
      ["keyRadius", "Key radius", 0, 40],
      ["keyboardWidth", "Keyboard width", 40, 100, "%"],
      ["fieldPadding", "Field padding", 0, 30],
      ["fieldThickness", "Field stroke", 1, 6],
      ["cursorThickness", "Cursor stroke", 1, 8],
    ],
    toggles: [
      ["fillKeys", "Fill keys"],
      ["outlineKeys", "Outline all keys"],
    ],
  },
  {
    value: "hints",
    title: "Hints and status",
    fields: [
      ["hintsHeight", "Bottom hints", 20, 100],
      ["sideHintsWidth", "Side hints", 16, 80],
      ["statusMarginX", "Status margin X", 0, 80],
      ["statusMarginY", "Status margin Y", 0, 80],
      ["progressHeight", "Progress height", 2, 40],
    ],
  },
];

function RangeField({ field, label, min, max, suffix, value, onChange }) {
  const set = (next) =>
    onChange(field, Math.max(min, Math.min(max, Number(next) || 0)));
  return (
    <div className="range-field">
      <div className="range-heading">
        <Label htmlFor={field}>{label}</Label>
        <div className="value-input">
          <Input
            id={field}
            type="number"
            min={min}
            max={max}
            value={value}
            onChange={(e) => set(e.target.value)}
          />
          <span>{suffix || "px"}</span>
        </div>
      </div>
      <Slider
        min={min}
        max={max}
        step={1}
        value={[value]}
        onValueChange={(values) => set(values[0])}
        aria-label={label}
      />
    </div>
  );
}

function ToggleField({ field, label, checked, onChange }) {
  return (
    <div className="toggle-field">
      <Label htmlFor={field}>{label}</Label>
      <Switch
        id={field}
        checked={checked}
        onCheckedChange={(next) => onChange(field, next)}
      />
    </div>
  );
}

function monochromeBmp(canvas) {
  const width = canvas.width,
    height = canvas.height,
    rowBytes = Math.ceil(width / 32) * 4,
    offset = 62;
  const bytes = new Uint8Array(offset + rowBytes * height),
    view = new DataView(bytes.buffer);
  bytes.set([0x42, 0x4d]);
  view.setUint32(2, bytes.length, true);
  view.setUint32(10, offset, true);
  view.setUint32(14, 40, true);
  view.setInt32(18, width, true);
  view.setInt32(22, height, true);
  view.setUint16(26, 1, true);
  view.setUint16(28, 1, true);
  view.setUint32(34, rowBytes * height, true);
  view.setUint32(46, 2, true);
  bytes.set([0, 0, 0, 0, 255, 255, 255, 0], 54);
  const pixels = canvas.getContext("2d").getImageData(0, 0, width, height).data;
  for (let y = 0; y < height; y += 1)
    for (let x = 0; x < width; x += 1) {
      const source = ((height - 1 - y) * width + x) * 4,
        light =
          pixels[source] * 0.299 +
          pixels[source + 1] * 0.587 +
          pixels[source + 2] * 0.114;
      if (light >= 128)
        bytes[offset + y * rowBytes + (x >> 3)] |= 0x80 >> (x & 7);
    }
  return bytes;
}

async function convertBackground(file) {
  const url = URL.createObjectURL(file);
  try {
    const image = new Image();
    image.src = url;
    await new Promise((resolve, reject) => {
      image.onload = resolve;
      image.onerror = reject;
    });
    const canvas = document.createElement("canvas");
    canvas.width = 800;
    canvas.height = 480;
    const context = canvas.getContext("2d");
    context.fillStyle = "white";
    context.fillRect(0, 0, 800, 480);
    const scale = Math.min(800 / image.width, 480 / image.height),
      width = image.width * scale,
      height = image.height * scale;
    context.drawImage(
      image,
      (800 - width) / 2,
      (480 - height) / 2,
      width,
      height,
    );
    return {
      bmp: monochromeBmp(canvas),
      preview: canvas.toDataURL("image/png"),
    };
  } finally {
    URL.revokeObjectURL(url);
  }
}

function HomePreview({ theme, background }) {
  const scale = 0.5,
    coverAreaTop = theme.homeTopPadding * scale;
  const coverStyle = {
    left: (theme.coverX / 1000) * 240,
    top:
      (theme.homeTopPadding +
        (theme.coverY / 1000) * theme.homeCoverAreaHeight) *
      scale,
    width: (theme.coverWidth / 1000) * 240,
    height: (theme.coverHeight / 1000) * theme.homeCoverAreaHeight * scale,
    borderRadius: theme.cornerRadius * scale,
  };
  return (
    <div className="device">
      <div
        className="device-header"
        style={{
          top: theme.topPadding * scale,
          height: theme.homeTopPadding * scale,
          paddingInline: theme.sidePadding * scale,
        }}
      >
        <b>Library</b>
        <span>10:42&nbsp; 82%</span>
      </div>
      <div
        className="home-bg"
        style={{
          top: coverAreaTop,
          height: theme.homeCoverAreaHeight * scale,
          backgroundImage: background ? `url(${background})` : undefined,
        }}
      />
      <div className="book-cover" style={coverStyle}>
        Recent book
      </div>
      <div
        className="device-menu"
        style={{
          top:
            (theme.homeTopPadding +
              theme.homeCoverAreaHeight +
              theme.homeMenuTopOffset) *
            scale,
          gap: theme.gap * scale,
          gridTemplateColumns: `repeat(${theme.columns},1fr)`,
        }}
      >
        {["Library", "Settings", "Wi-Fi", "Sleep"].map((item) => (
          <span key={item} style={{ height: theme.rowHeight * scale }}>
            {item}
          </span>
        ))}
      </div>
      <div
        className="device-hints"
        style={{ height: theme.hintsHeight * scale }}
      >
        {["Back", "Select", "Open", "Menu"].map((x) => (
          <span key={x}>{x}</span>
        ))}
      </div>
    </div>
  );
}

function Preview({ theme, background }) {
  const keyStyle = {
    width: theme.keyWidth,
    height: theme.keyHeight,
    borderRadius: theme.keyRadius,
    margin: theme.keySpacing / 2,
  };
  return (
    <Tabs defaultValue="home">
      <TabsList className="preview-tabs">
        <TabsTrigger value="home">Home</TabsTrigger>
        <TabsTrigger value="navigation">Navigation</TabsTrigger>
        <TabsTrigger value="dialogs">Dialogs</TabsTrigger>
        <TabsTrigger value="reader">Reader</TabsTrigger>
      </TabsList>
      <TabsContent value="home">
        <HomePreview theme={theme} background={background} />
      </TabsContent>
      <TabsContent value="navigation">
        <div className="component-screen">
          <div
            className="sample-header"
            style={{
              height: theme.headerHeight,
              padding: `${theme.topPadding}px ${theme.sidePadding}px`,
            }}
          >
            <b>Settings</b>
            <span>82%</span>
          </div>
          <div
            className="sample-tabs"
            style={{ height: theme.tabHeight, gap: theme.tabSpacing }}
          >
            <b>Display</b>
            <span>Reader</span>
            <span>System</span>
          </div>
          <div
            className="sample-list"
            style={{ paddingInline: theme.sidePadding }}
          >
            <span style={{ height: theme.listRowHeight }}>Theme</span>
            <span className="selected" style={{ height: theme.listRowHeight }}>
              Custom theme
            </span>
            <span style={{ height: theme.subtitleRowHeight }}>
              Font<small>Lexend Deca</small>
            </span>
          </div>
        </div>
      </TabsContent>
      <TabsContent value="dialogs">
        <div className="component-screen dialog-screen">
          <div
            className={
              theme.popupInverted ? "sample-popup inverted" : "sample-popup"
            }
            style={{
              marginTop: `${theme.popupTop / 10}%`,
              padding: `${theme.popupMarginY}px ${theme.popupMarginX}px`,
              borderRadius: theme.popupRadius,
              borderWidth: theme.popupFrame,
              fontWeight: theme.popupBold ? 700 : 400,
            }}
          >
            Theme saved
            <div style={{ height: theme.popupProgress }} />
          </div>
          <div
            className="sample-input"
            style={{
              width: `${theme.keyboardWidth}%`,
              borderBottomWidth: theme.fieldThickness,
              padding: theme.fieldPadding,
            }}
          >
            Theme name
            <div>
              {["A", "B", "C", "⌫"].map((x, i) => (
                <kbd
                  key={x}
                  className={`${i === 1 ? "selected " : ""}${theme.fillKeys ? "filled " : ""}${theme.outlineKeys ? "outlined" : ""}`}
                  style={keyStyle}
                >
                  {x}
                </kbd>
              ))}
            </div>
          </div>
        </div>
      </TabsContent>
      <TabsContent value="reader">
        <div className="reader-screen">
          <div className="reader-page">A quiet place to read.</div>
          <div
            className="reader-status"
            style={{
              paddingInline: theme.statusMarginX,
              paddingBottom: theme.statusMarginY,
            }}
          >
            <span>82%</span>
            <span>Chapter one</span>
            <span>42 / 210</span>
            <i style={{ height: theme.progressHeight }} />
          </div>
        </div>
      </TabsContent>
    </Tabs>
  );
}

export default function App() {
  const [theme, setTheme] = useState(defaults),
    [asset, setAsset] = useState(null),
    [message, setMessage] = useState("");
  const update = (field, value) =>
    setTheme((current) => ({ ...current, [field]: value }));
  const valid = useMemo(
    () => /^[a-z0-9-]{1,32}$/.test(theme.id) && theme.name.trim().length > 0,
    [theme.id, theme.name],
  );
  const selectImage = async (event) => {
    const file = event.target.files[0];
    if (!file) return;
    try {
      setMessage("Converting background…");
      setAsset(await convertBackground(file));
      setMessage("Background ready — converted locally to a 1-bit BMP.");
    } catch {
      setAsset(null);
      setMessage("Could not read that image.");
    }
  };
  const exportTheme = async () => {
    if (!valid) {
      setMessage("Add a name and a valid lowercase identifier.");
      return;
    }
    const manifest = {
      schemaVersion: 2,
      engine: "declarative",
      id: theme.id,
      name: theme.name.trim(),
      home: {
        topPadding: theme.homeTopPadding,
        coverAreaHeight: theme.homeCoverAreaHeight,
        menuTopOffset: theme.homeMenuTopOffset,
        cover: {
          x: theme.coverX,
          y: theme.coverY,
          width: theme.coverWidth,
          height: theme.coverHeight,
          cornerRadius: theme.cornerRadius,
        },
      },
      header: {
        height: theme.headerHeight,
        topPadding: theme.topPadding,
        spacing: theme.verticalSpacing,
        sidePadding: theme.sidePadding,
        tabHeight: theme.tabHeight,
        tabSpacing: theme.tabSpacing,
      },
      list: {
        rowHeight: theme.listRowHeight,
        subtitleRowHeight: theme.subtitleRowHeight,
      },
      menu: {
        columns: theme.columns,
        rowHeight: theme.rowHeight,
        gap: theme.gap,
      },
      popup: {
        top: theme.popupTop,
        marginX: theme.popupMarginX,
        marginY: theme.popupMarginY,
        cornerRadius: theme.popupRadius,
        frameThickness: theme.popupFrame,
        progressHeight: theme.popupProgress,
        bold: theme.popupBold,
        inverted: theme.popupInverted,
      },
      input: {
        keyWidth: theme.keyWidth,
        keyHeight: theme.keyHeight,
        keySpacing: theme.keySpacing,
        cornerRadius: theme.keyRadius,
        widthPercent: theme.keyboardWidth,
        textFieldPadding: theme.fieldPadding,
        textFieldThickness: theme.fieldThickness,
        cursorThickness: theme.cursorThickness,
        fillUnselected: theme.fillKeys,
        outlineUnselected: theme.outlineKeys,
      },
      hints: { height: theme.hintsHeight, sideWidth: theme.sideHintsWidth },
      status: {
        marginX: theme.statusMarginX,
        marginY: theme.statusMarginY,
        progressHeight: theme.progressHeight,
      },
    };
    const zip = new JSZip();
    if (asset) {
      manifest.home.background = "assets/background.bmp";
      zip.file("assets/background.bmp", asset.bmp);
    }
    zip.file("theme.json", JSON.stringify(manifest, null, 2));
    const blob = await zip.generateAsync({
        type: "blob",
        compression: "DEFLATE",
      }),
      url = URL.createObjectURL(blob),
      link = document.createElement("a");
    link.href = url;
    link.download = `${theme.id}.cptheme`;
    link.click();
    setTimeout(() => URL.revokeObjectURL(url), 1000);
    setMessage(
      "Package created. Import it from the Themes page on your device.",
    );
  };
  return (
    <main>
      <header className="hero">
        <div>
          <Badge>
            <Sparkles size={13} /> CrossInk community tool
          </Badge>
          <h1>Theme Studio</h1>
          <p>
            Shape every CrossInk interface component and export a safe
            declarative theme — entirely in your browser.
          </p>
        </div>
        <div className="hero-actions">
          <Button
            variant="outline"
            onClick={() => {
              setTheme(defaults);
              setAsset(null);
              setMessage("Defaults restored.");
            }}
          >
            <RotateCcw size={16} /> Reset
          </Button>
          <Button onClick={exportTheme} disabled={!valid}>
            <Download size={16} /> Export .cptheme
          </Button>
        </div>
      </header>
      <div className="workspace">
        <Card className="editor-card">
          <CardHeader>
            <CardTitle>Design system</CardTitle>
            <CardDescription>
              Controls are grouped by the firmware component they affect.
            </CardDescription>
          </CardHeader>
          <CardContent>
            <div className="metadata">
              <div>
                <Label htmlFor="name">Theme name</Label>
                <Input
                  id="name"
                  value={theme.name}
                  maxLength={48}
                  onChange={(e) => update("name", e.target.value)}
                />
              </div>
              <div>
                <Label htmlFor="id">Identifier</Label>
                <Input
                  id="id"
                  value={theme.id}
                  maxLength={32}
                  aria-invalid={!valid}
                  onChange={(e) =>
                    update(
                      "id",
                      e.target.value.toLowerCase().replace(/[^a-z0-9-]/g, ""),
                    )
                  }
                />
              </div>
            </div>
            <label className="upload">
              <ImagePlus size={20} />
              <span>
                <b>{asset ? "Change background" : "Add a background image"}</b>
                <small>Converted locally to 800×480 monochrome BMP</small>
              </span>
              <Input type="file" accept="image/*" onChange={selectImage} />
            </label>
            <Accordion
              type="multiple"
              defaultValue={["home", "menu"]}
              className="controls"
            >
              {groups.map((group) => (
                <AccordionItem key={group.value} value={group.value}>
                  <AccordionTrigger>{group.title}</AccordionTrigger>
                  <AccordionContent>
                    <div className="range-grid">
                      {group.fields.map(([field, label, min, max, suffix]) => (
                        <RangeField
                          key={field}
                          field={field}
                          label={label}
                          min={min}
                          max={max}
                          suffix={suffix}
                          value={theme[field]}
                          onChange={update}
                        />
                      ))}
                    </div>
                    {group.toggles?.map(([field, label]) => (
                      <ToggleField
                        key={field}
                        field={field}
                        label={label}
                        checked={theme[field]}
                        onChange={update}
                      />
                    ))}
                  </AccordionContent>
                </AccordionItem>
              ))}
            </Accordion>
            {message && (
              <p className="message" role="status">
                {message}
              </p>
            )}
          </CardContent>
        </Card>
        <Card className="preview-card">
          <CardHeader>
            <div>
              <CardTitle>
                <MonitorSmartphone size={18} /> Live preview
              </CardTitle>
              <CardDescription>
                480×800 device geometry and component states.
              </CardDescription>
            </div>
            <Badge>v2 declarative</Badge>
          </CardHeader>
          <CardContent>
            <Preview theme={theme} background={asset?.preview} />
          </CardContent>
        </Card>
      </div>
    </main>
  );
}
