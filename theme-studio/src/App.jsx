import { useMemo, useState } from "react";
import JSZip from "jszip";
import { Download, MonitorSmartphone, RotateCcw, Sparkles } from "lucide-react";
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
  homeCoverAreaHeight: 340,
  homeMenuTopOffset: 12,
  homeLayout: "shelf",
  homeRecentBooks: 3,
  homeBookGap: 10,
  homeShowCover: true,
  homeShowTitle: true,
  homeShowAuthor: false,
  homeShowProgress: true,
  homeShowBookStats: false,
  homeShowGlobalStats: true,
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
  settingsLayout: "grid",
  settingsColumns: 2,
  settingsGap: 8,
  settingsCardRadius: 8,
  settingsCardHeight: 64,
  settingsOrder: "uiTheme, sleepScreen",
};

const groups = [
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

function HomePreview({ theme }) {
  const scale = 0.5;
  const books = [
    ["The Left Hand of Darkness", "Ursula K. Le Guin"],
    ["Kindred", "Octavia E. Butler"],
    ["The Dispossessed", "Ursula K. Le Guin"],
  ].slice(0, theme.homeRecentBooks);
  const bookCards = books.map(([title, author], index) => (
    <div
      className={`home-book-card ${index === 0 ? "selected" : ""}`}
      style={{ borderRadius: theme.cornerRadius * scale }}
      key={title}
    >
      {theme.homeShowCover && (
        <div className={`preview-cover cover-${index + 1}`} />
      )}
      <div className="home-book-copy">
        {theme.homeShowTitle && <b>{title}</b>}
        {theme.homeShowAuthor && <small>{author}</small>}
        {theme.homeShowProgress && index === 0 && (
          <i className="home-progress">
            <span />
          </i>
        )}
      </div>
    </div>
  ));
  const statModule = (type) => (
    <div className="home-stats" key={type}>
      {(type === "book"
        ? [
            ["4h 20m", "Reading time"],
            ["42%", "Progress"],
            ["186", "Pages"],
          ]
        : [
            ["28h", "All books"],
            ["7", "Completed"],
            ["21", "Sessions"],
          ]
      ).map(([value, label]) => (
        <span key={label}>
          <b>{value}</b>
          <small>{label}</small>
        </span>
      ))}
    </div>
  );
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
        className={`home-composition home-${theme.homeLayout}`}
        style={{
          top: theme.homeTopPadding * scale,
          height: theme.homeCoverAreaHeight * scale,
          gap: theme.homeBookGap * scale,
        }}
      >
        <div className="home-books" style={{ gap: theme.homeBookGap * scale }}>
          {theme.homeLayout === "shelf" ? bookCards : bookCards[0]}
        </div>
        {theme.homeLayout !== "shelf" && theme.homeRecentBooks > 1 && (
          <div className="home-book-nav">
            ‹&nbsp; 1 / {theme.homeRecentBooks} &nbsp;›
          </div>
        )}
        {theme.homeShowBookStats && statModule("book")}
        {theme.homeShowGlobalStats && statModule("global")}
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

function Preview({ theme }) {
  const keyStyle = {
    width: theme.keyWidth,
    height: theme.keyHeight,
    borderRadius: theme.keyRadius,
    margin: theme.keySpacing / 2,
  };
  const preferredSettings = theme.settingsOrder
    .split(/[\s,]+/)
    .map((item) => item.trim())
    .filter(Boolean);
  const settingsItems = [
    ["uiTheme", "UI theme", "My Theme"],
    ["sleepScreen", "Sleep screen", "Cover"],
    ["refreshFrequency", "Screen refresh", "7"],
    ["hideClock", "Show clock", "On"],
    ["language", "Language", "English"],
    ["device", "Device", ">"],
  ].sort((first, second) => {
    const firstRank = preferredSettings.indexOf(first[0]);
    const secondRank = preferredSettings.indexOf(second[0]);
    if (firstRank < 0 && secondRank < 0) return 0;
    if (firstRank < 0) return 1;
    if (secondRank < 0) return -1;
    return firstRank - secondRank;
  });
  return (
    <Tabs defaultValue="home">
      <TabsList className="preview-tabs">
        <TabsTrigger value="home">Home</TabsTrigger>
        <TabsTrigger value="navigation">Navigation</TabsTrigger>
        <TabsTrigger value="dialogs">Dialogs</TabsTrigger>
        <TabsTrigger value="reader">Reader</TabsTrigger>
      </TabsList>
      <TabsContent value="home">
        <HomePreview theme={theme} />
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
            className={`settings-preview settings-${theme.settingsLayout}`}
            style={{
              paddingInline: theme.sidePadding,
              gap: theme.settingsLayout === "list" ? 0 : theme.settingsGap,
              gridTemplateColumns:
                theme.settingsLayout === "grid"
                  ? `repeat(${theme.settingsColumns}, 1fr)`
                  : "1fr",
            }}
          >
            {settingsItems.map(([, label, value], index) => (
              <span
                key={label}
                className={index === 1 ? "selected" : ""}
                style={{
                  minHeight:
                    theme.settingsLayout === "list"
                      ? theme.listRowHeight
                      : theme.settingsCardHeight,
                  borderRadius:
                    theme.settingsLayout === "list"
                      ? 0
                      : theme.settingsCardRadius,
                }}
              >
                <b>{label}</b>
                <small>{value}</small>
              </span>
            ))}
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
    [message, setMessage] = useState("");
  const update = (field, value) =>
    setTheme((current) => ({ ...current, [field]: value }));
  const settingsOrder = useMemo(
    () =>
      theme.settingsOrder
        .split(/[\s,]+/)
        .map((item) => item.trim())
        .filter(Boolean),
    [theme.settingsOrder],
  );
  const valid = useMemo(
    () =>
      /^[a-z0-9-]{1,32}$/.test(theme.id) &&
      theme.name.trim().length > 0 &&
      settingsOrder.length <= 8 &&
      settingsOrder.every((item) => /^[A-Za-z0-9_-]{1,48}$/.test(item)) &&
      new Set(settingsOrder).size === settingsOrder.length,
    [settingsOrder, theme.id, theme.name],
  );
  const exportTheme = async () => {
    if (!valid) {
      setMessage("Add a name and a valid lowercase identifier.");
      return;
    }
    const manifest = {
      schemaVersion: 3,
      engine: "declarative",
      id: theme.id,
      name: theme.name.trim(),
      home: {
        topPadding: theme.homeTopPadding,
        coverAreaHeight: theme.homeCoverAreaHeight,
        menuTopOffset: theme.homeMenuTopOffset,
        layout: theme.homeLayout,
        recentBooks: theme.homeRecentBooks,
        bookGap: theme.homeBookGap,
        showCover: theme.homeShowCover,
        showTitle: theme.homeShowTitle,
        showAuthor: theme.homeShowAuthor,
        showProgress: theme.homeShowProgress,
        showBookStats: theme.homeShowBookStats,
        showGlobalStats: theme.homeShowGlobalStats,
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
      screens: {
        settings: {
          layout: theme.settingsLayout,
          columns: theme.settingsLayout === "cards" ? 1 : theme.settingsColumns,
          gap: theme.settingsGap,
          cardRadius: theme.settingsCardRadius,
          cardHeight: theme.settingsCardHeight,
          order: settingsOrder,
        },
      },
    };
    const zip = new JSZip();
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
            <Accordion
              type="multiple"
              defaultValue={["settings", "home", "menu"]}
              className="controls"
            >
              <AccordionItem value="home">
                <AccordionTrigger>Home composition</AccordionTrigger>
                <AccordionContent>
                  <div
                    className="layout-picker"
                    role="group"
                    aria-label="Home layout"
                  >
                    {["shelf", "spotlight", "dashboard"].map((layout) => (
                      <Button
                        key={layout}
                        type="button"
                        size="sm"
                        variant={
                          theme.homeLayout === layout ? "default" : "outline"
                        }
                        onClick={() => update("homeLayout", layout)}
                      >
                        {layout[0].toUpperCase() + layout.slice(1)}
                      </Button>
                    ))}
                  </div>
                  <div className="range-grid">
                    <RangeField
                      field="homeRecentBooks"
                      label="Recent books"
                      min={1}
                      max={3}
                      value={theme.homeRecentBooks}
                      onChange={update}
                    />
                    <RangeField
                      field="homeCoverAreaHeight"
                      label="Composition height"
                      min={180}
                      max={600}
                      value={theme.homeCoverAreaHeight}
                      onChange={update}
                    />
                    <RangeField
                      field="homeBookGap"
                      label="Module gap"
                      min={0}
                      max={40}
                      value={theme.homeBookGap}
                      onChange={update}
                    />
                    <RangeField
                      field="homeMenuTopOffset"
                      label="Menu offset"
                      min={0}
                      max={100}
                      value={theme.homeMenuTopOffset}
                      onChange={update}
                    />
                    <RangeField
                      field="coverHeight"
                      label="Cover scale"
                      min={300}
                      max={1000}
                      value={theme.coverHeight}
                      onChange={update}
                    />
                    <RangeField
                      field="cornerRadius"
                      label="Corner radius"
                      min={0}
                      max={40}
                      value={theme.cornerRadius}
                      onChange={update}
                    />
                  </div>
                  <div className="module-toggles">
                    {[
                      ["homeShowCover", "Book covers"],
                      ["homeShowTitle", "Book titles"],
                      ["homeShowAuthor", "Authors"],
                      ["homeShowProgress", "Selected book progress"],
                      ["homeShowBookStats", "Selected book statistics"],
                      ["homeShowGlobalStats", "Global reading statistics"],
                    ].map(([field, label]) => (
                      <ToggleField
                        key={field}
                        field={field}
                        label={label}
                        checked={theme[field]}
                        onChange={update}
                      />
                    ))}
                  </div>
                  {theme.homeLayout === "spotlight" && (
                    <details className="advanced-home">
                      <summary>Advanced spotlight geometry</summary>
                      <div className="range-grid">
                        <RangeField
                          field="coverX"
                          label="Cover X"
                          min={0}
                          max={1000}
                          value={theme.coverX}
                          onChange={update}
                        />
                        <RangeField
                          field="coverY"
                          label="Cover Y"
                          min={0}
                          max={1000}
                          value={theme.coverY}
                          onChange={update}
                        />
                        <RangeField
                          field="coverWidth"
                          label="Cover width"
                          min={1}
                          max={1000}
                          value={theme.coverWidth}
                          onChange={update}
                        />
                      </div>
                    </details>
                  )}
                </AccordionContent>
              </AccordionItem>
              <AccordionItem value="settings">
                <AccordionTrigger>Settings screen structure</AccordionTrigger>
                <AccordionContent>
                  <div
                    className="layout-picker"
                    role="group"
                    aria-label="Settings layout"
                  >
                    {["list", "cards", "grid"].map((layout) => (
                      <Button
                        key={layout}
                        type="button"
                        size="sm"
                        variant={
                          theme.settingsLayout === layout
                            ? "default"
                            : "outline"
                        }
                        onClick={() => update("settingsLayout", layout)}
                      >
                        {layout[0].toUpperCase() + layout.slice(1)}
                      </Button>
                    ))}
                  </div>
                  <div className="range-grid">
                    {theme.settingsLayout === "grid" && (
                      <RangeField
                        field="settingsColumns"
                        label="Columns"
                        min={1}
                        max={3}
                        value={theme.settingsColumns}
                        onChange={update}
                      />
                    )}
                    <RangeField
                      field="settingsGap"
                      label="Gap"
                      min={0}
                      max={40}
                      value={theme.settingsGap}
                      onChange={update}
                    />
                    <RangeField
                      field="settingsCardHeight"
                      label="Card height"
                      min={44}
                      max={140}
                      value={theme.settingsCardHeight}
                      onChange={update}
                    />
                    <RangeField
                      field="settingsCardRadius"
                      label="Card radius"
                      min={0}
                      max={40}
                      value={theme.settingsCardRadius}
                      onChange={update}
                    />
                  </div>
                  <div className="order-field">
                    <Label htmlFor="settings-order">
                      Preferred setting order
                    </Label>
                    <textarea
                      id="settings-order"
                      value={theme.settingsOrder}
                      aria-invalid={!valid}
                      onChange={(event) =>
                        update("settingsOrder", event.target.value)
                      }
                      placeholder="uiTheme, sleepScreen"
                    />
                    <small>
                      Up to 8 stable setting keys, separated by commas. Other
                      settings remain visible after them.
                    </small>
                  </div>
                </AccordionContent>
              </AccordionItem>
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
            <Badge>v3 declarative</Badge>
          </CardHeader>
          <CardContent>
            <Preview theme={theme} />
          </CardContent>
        </Card>
      </div>
    </main>
  );
}
