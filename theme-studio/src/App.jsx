import { useEffect, useMemo, useRef, useState } from "react";
import JSZip from "jszip";
import {
  AlertCircle,
  Check,
  CheckCircle2,
  ChevronDown,
  ChevronUp,
  Download,
  Home,
  Layers3,
  LayoutTemplate,
  MonitorSmartphone,
  Palette,
  Plus,
  RotateCcw,
  Settings2,
  SlidersHorizontal,
  Trash2,
  Upload,
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
  homeMenuPresentation: "panel",
  homePanelColumns: 2,
  homeActionOrder: [
    "browse",
    "recents",
    "opds",
    "stats",
    "saved",
    "transfer",
    "settings",
  ],
  homePinnedActions: ["browse"],
  homeCanvasBlocks: [
    {
      id: "recents",
      type: "recentBooks",
      variant: "cards",
      x: 30,
      y: 20,
      width: 940,
      height: 520,
    },
    {
      id: "global",
      type: "globalStats",
      x: 30,
      y: 570,
      width: 650,
      height: 150,
    },
  ],
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

const canvasBlockCatalog = [
  ["recentBooks", "Recent books"],
  ["bookProgress", "Book progress"],
  ["bookStats", "Book statistics"],
  ["globalStats", "Global statistics"],
  ["quickActions", "Quick actions"],
  ["menuTrigger", "Menu button"],
];

const canvasBlockDetails = {
  recentBooks: "Browse up to three recent books.",
  bookProgress: "Current book percentage and progress bar.",
  bookStats: "Reading time, progress and pages.",
  globalStats: "Total time, completed books and sessions.",
  quickActions: "Inline actions or up to three pinned shortcuts.",
  menuTrigger: "A visible shortcut to the complete action panel.",
};

const homeLayoutCatalog = [
  {
    id: "shelf",
    label: "Book shelf",
    description: "Show up to three books at once.",
  },
  {
    id: "spotlight",
    label: "Focus",
    description: "Center one selected book.",
  },
  {
    id: "dashboard",
    label: "Side-by-side",
    description: "Cover left, book details right.",
  },
];

const deviceProfiles = {
  x4: { label: "X4", width: 480, height: 800 },
  x3: { label: "X3", width: 528, height: 792 },
};

const homeActionCatalog = [
  ["browse", "Browse files"],
  ["recents", "Recent books"],
  ["opds", "OPDS"],
  ["stats", "Reading statistics"],
  ["saved", "Saved items"],
  ["transfer", "File transfer"],
  ["settings", "Settings"],
];

const studioSections = [
  {
    id: "overview",
    label: "Theme setup",
    description: "Name, identifier and file",
    icon: Palette,
  },
  {
    id: "home",
    label: "Home screen",
    description: "Blocks, books and actions",
    icon: Home,
  },
  {
    id: "components",
    label: "Components",
    description: "Navigation, dialogs and input",
    icon: SlidersHorizontal,
  },
  {
    id: "settings",
    label: "Settings screen",
    description: "Layout and item order",
    icon: Settings2,
  },
];

const getInitialTheme = () => {
  try {
    const draft = window.localStorage.getItem("crossink-theme-studio-draft-v4");
    if (!draft) return defaults;
    const parsed = JSON.parse(draft);
    return { ...defaults, ...parsed };
  } catch {
    return defaults;
  }
};

function themeFromManifest(manifest) {
  const home = manifest.home || {};
  const actions = home.actions || {};
  const cover = home.cover || {};
  const header = manifest.header || {};
  const list = manifest.list || {};
  const menu = manifest.menu || {};
  const popup = manifest.popup || {};
  const input = manifest.input || {};
  const hints = manifest.hints || {};
  const status = manifest.status || {};
  const settings = manifest.screens?.settings || {};
  const blocks = Array.isArray(home.blocks)
    ? home.blocks.slice(0, 8).map((block, index) => ({
        id: `${block.type || "block"}-${index}`,
        type: block.type,
        ...(block.type === "recentBooks"
          ? { variant: block.variant || "cards" }
          : {}),
        x: block.frame?.x ?? 0,
        y: block.frame?.y ?? 0,
        width: block.frame?.width ?? 400,
        height: block.frame?.height ?? 180,
      }))
    : defaults.homeCanvasBlocks;

  return {
    ...defaults,
    name: manifest.name || defaults.name,
    id: manifest.id || defaults.id,
    homeTopPadding: home.topPadding ?? defaults.homeTopPadding,
    homeCoverAreaHeight: home.coverAreaHeight ?? defaults.homeCoverAreaHeight,
    homeMenuTopOffset: home.menuTopOffset ?? defaults.homeMenuTopOffset,
    homeLayout: home.layout || defaults.homeLayout,
    homeRecentBooks: home.recentBooks ?? defaults.homeRecentBooks,
    homeBookGap: home.bookGap ?? defaults.homeBookGap,
    homeShowCover: home.showCover ?? defaults.homeShowCover,
    homeShowTitle: home.showTitle ?? defaults.homeShowTitle,
    homeShowAuthor: home.showAuthor ?? defaults.homeShowAuthor,
    homeShowProgress: home.showProgress ?? defaults.homeShowProgress,
    homeShowBookStats: home.showBookStats ?? defaults.homeShowBookStats,
    homeShowGlobalStats: home.showGlobalStats ?? defaults.homeShowGlobalStats,
    homeMenuPresentation: actions.presentation || defaults.homeMenuPresentation,
    homePanelColumns: actions.panel?.columns ?? defaults.homePanelColumns,
    homeActionOrder: Array.isArray(actions.order)
      ? actions.order
      : defaults.homeActionOrder,
    homePinnedActions: Array.isArray(actions.pinned)
      ? actions.pinned
      : defaults.homePinnedActions,
    homeCanvasBlocks: blocks,
    coverX: cover.x ?? defaults.coverX,
    coverY: cover.y ?? defaults.coverY,
    coverWidth: cover.width ?? defaults.coverWidth,
    coverHeight: cover.height ?? defaults.coverHeight,
    cornerRadius: cover.cornerRadius ?? defaults.cornerRadius,
    columns: menu.columns ?? defaults.columns,
    rowHeight: menu.rowHeight ?? defaults.rowHeight,
    gap: menu.gap ?? defaults.gap,
    headerHeight: header.height ?? defaults.headerHeight,
    topPadding: header.topPadding ?? defaults.topPadding,
    verticalSpacing: header.spacing ?? defaults.verticalSpacing,
    sidePadding: header.sidePadding ?? defaults.sidePadding,
    tabHeight: header.tabHeight ?? defaults.tabHeight,
    tabSpacing: header.tabSpacing ?? defaults.tabSpacing,
    listRowHeight: list.rowHeight ?? defaults.listRowHeight,
    subtitleRowHeight: list.subtitleRowHeight ?? defaults.subtitleRowHeight,
    popupTop: popup.top ?? defaults.popupTop,
    popupMarginX: popup.marginX ?? defaults.popupMarginX,
    popupMarginY: popup.marginY ?? defaults.popupMarginY,
    popupRadius: popup.cornerRadius ?? defaults.popupRadius,
    popupFrame: popup.frameThickness ?? defaults.popupFrame,
    popupProgress: popup.progressHeight ?? defaults.popupProgress,
    popupBold: popup.bold ?? defaults.popupBold,
    popupInverted: popup.inverted ?? defaults.popupInverted,
    keyWidth: input.keyWidth ?? defaults.keyWidth,
    keyHeight: input.keyHeight ?? defaults.keyHeight,
    keySpacing: input.keySpacing ?? defaults.keySpacing,
    keyRadius: input.cornerRadius ?? defaults.keyRadius,
    keyboardWidth: input.widthPercent ?? defaults.keyboardWidth,
    fieldPadding: input.textFieldPadding ?? defaults.fieldPadding,
    fieldThickness: input.textFieldThickness ?? defaults.fieldThickness,
    cursorThickness: input.cursorThickness ?? defaults.cursorThickness,
    fillKeys: input.fillUnselected ?? defaults.fillKeys,
    outlineKeys: input.outlineUnselected ?? defaults.outlineKeys,
    hintsHeight: hints.height ?? defaults.hintsHeight,
    sideHintsWidth: hints.sideWidth ?? defaults.sideHintsWidth,
    statusMarginX: status.marginX ?? defaults.statusMarginX,
    statusMarginY: status.marginY ?? defaults.statusMarginY,
    progressHeight: status.progressHeight ?? defaults.progressHeight,
    settingsLayout: settings.layout || defaults.settingsLayout,
    settingsColumns: settings.columns ?? defaults.settingsColumns,
    settingsGap: settings.gap ?? defaults.settingsGap,
    settingsCardRadius: settings.cardRadius ?? defaults.settingsCardRadius,
    settingsCardHeight: settings.cardHeight ?? defaults.settingsCardHeight,
    settingsOrder: Array.isArray(settings.order)
      ? settings.order.join(", ")
      : defaults.settingsOrder,
  };
}

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

const clamp = (value, min, max) => Math.max(min, Math.min(max, value));
const snap = (value) => Math.round(value / 10) * 10;

const framesOverlap = (first, second) =>
  first.x < second.x + second.width &&
  first.x + first.width > second.x &&
  first.y < second.y + second.height &&
  first.y + first.height > second.y;

function findCanvasPlacement(blocks, width, height) {
  for (let y = 20; y + height <= 1000; y += 40) {
    for (let x = 30; x + width <= 1000; x += 40) {
      const candidate = { x, y, width, height };
      if (!blocks.some((block) => framesOverlap(candidate, block))) {
        return { x, y };
      }
    }
  }
  const offset = (blocks.length * 50) % 240;
  return {
    x: clamp(30 + offset, 0, 1000 - width),
    y: clamp(30 + offset, 0, 1000 - height),
  };
}

const suggestedCanvasSize = (type) => {
  const sizes = {
    recentBooks: [940, 440],
    bookProgress: [280, 110],
    bookStats: [450, 140],
    globalStats: [450, 140],
    quickActions: [450, 140],
    menuTrigger: [260, 100],
  };
  return sizes[type] || [400, 140];
};

function arrangeCanvasBlocks(blocks) {
  return blocks.reduce((arranged, block) => {
    const [width, height] = suggestedCanvasSize(block.type);
    const position = findCanvasPlacement(arranged, width, height);
    return [...arranged, { ...block, ...position, width, height }];
  }, []);
}

function CanvasBookCards({ theme, block, selectedBookIndex }) {
  const books = ["Left Hand of Darkness", "Kindred", "The Dispossessed"].slice(
    0,
    theme.homeRecentBooks,
  );
  const visibleBooks =
    theme.homeLayout === "shelf"
      ? books
      : [books[selectedBookIndex % books.length]];
  return (
    <div
      className={`canvas-books canvas-books-${theme.homeLayout} canvas-books-${block.variant || "cards"}`}
      style={{ gap: `${Math.max(0, theme.homeBookGap * 0.5)}px` }}
    >
      {visibleBooks.map((title) => {
        const bookIndex = books.indexOf(title);
        const selected = bookIndex === selectedBookIndex;
        return (
          <span className={selected ? "active" : ""} key={title}>
            {theme.homeShowCover && <i className={`cover-${bookIndex + 1}`} />}
            <span className="canvas-book-copy">
              {theme.homeShowTitle && <b>{title}</b>}
              {theme.homeShowAuthor && <small>Ursula K. Le Guin</small>}
              {theme.homeShowProgress && selected && (
                <em>
                  <u />
                </em>
              )}
            </span>
          </span>
        );
      })}
      {theme.homeLayout !== "shelf" && books.length > 1 && (
        <small className="canvas-book-position">
          {selectedBookIndex + 1} / {books.length}
        </small>
      )}
    </div>
  );
}

function CanvasStatBlock({ global }) {
  const values = global
    ? [
        ["28h", "Read"],
        ["7", "Done"],
        ["21", "Sessions"],
      ]
    : [
        ["4h", "Time"],
        ["42%", "Progress"],
        ["186", "Pages"],
      ];
  return (
    <div className="canvas-stats">
      {values.map(([value, label]) => (
        <span key={label}>
          <b>{value}</b>
          <small>{label}</small>
        </span>
      ))}
    </div>
  );
}

function CanvasBlockContent({
  block,
  theme,
  testMode,
  focused,
  quickActionIndex,
  onOpenMenu,
  selectedBookIndex,
}) {
  if (block.type === "recentBooks")
    return (
      <CanvasBookCards
        theme={theme}
        block={block}
        selectedBookIndex={selectedBookIndex}
      />
    );
  if (block.type === "bookProgress")
    return (
      <div className="canvas-progress">
        <b>42%</b>
        <i>
          <u />
        </i>
      </div>
    );
  if (block.type === "bookStats") return <CanvasStatBlock />;
  if (block.type === "globalStats") return <CanvasStatBlock global />;
  if (block.type === "quickActions") {
    const visible =
      theme.homeMenuPresentation === "inline"
        ? theme.homeActionOrder
        : theme.homePinnedActions;
    const actions = visible
      .map((id) => homeActionCatalog.find(([action]) => action === id))
      .filter(Boolean);
    return (
      <div
        className={`canvas-actions ${theme.homeMenuPresentation === "inline" ? "inline" : ""}`}
        style={{ gap: `${Math.max(0, theme.homeBookGap * 0.5)}px` }}
      >
        {actions.map(([, label], index) => (
          <span
            className={
              testMode && focused && index === quickActionIndex ? "active" : ""
            }
            key={label}
          >
            {label}
          </span>
        ))}
      </div>
    );
  }
  return (
    <button type="button" onClick={testMode ? onOpenMenu : undefined}>
      Menu
    </button>
  );
}

function HomeCanvasEditor({
  theme,
  onChange,
  selectedBlock,
  onSelectBlock,
  onRemoveBlock,
  testMode,
  deviceProfile,
}) {
  const surfaceRef = useRef(null);
  const [gesture, setGesture] = useState(null);
  const [menuOpen, setMenuOpen] = useState(false);
  const [selectedBookIndex, setSelectedBookIndex] = useState(0);
  const [focusedBlockId, setFocusedBlockId] = useState(null);
  const [quickActionIndex, setQuickActionIndex] = useState(0);
  const [panelIndex, setPanelIndex] = useState(0);
  const visibleQuickActions =
    theme.homeMenuPresentation === "inline"
      ? theme.homeActionOrder
      : theme.homePinnedActions;
  const interactiveBlocks = theme.homeCanvasBlocks.filter(
    (block) =>
      block.type === "recentBooks" ||
      block.type === "menuTrigger" ||
      (block.type === "quickActions" && visibleQuickActions.length > 0),
  );
  const headerPercent = clamp(
    (theme.homeTopPadding / deviceProfile.height) * 100,
    4,
    20,
  );
  const hintsPercent = clamp(
    (theme.hintsHeight / deviceProfile.height) * 100,
    3,
    14,
  );
  useEffect(() => {
    if (!testMode) {
      setMenuOpen(false);
      return;
    }
    const selectedIsInteractive = interactiveBlocks.some(
      (block) => block.id === selectedBlock,
    );
    setFocusedBlockId(
      selectedIsInteractive ? selectedBlock : interactiveBlocks[0]?.id || null,
    );
    setPanelIndex(0);
    setQuickActionIndex(0);
  }, [testMode]);
  useEffect(() => {
    setSelectedBookIndex((current) =>
      Math.min(current, Math.max(0, theme.homeRecentBooks - 1)),
    );
  }, [theme.homeRecentBooks]);
  const updateBlock = (id, patch) => {
    onChange(
      "homeCanvasBlocks",
      theme.homeCanvasBlocks.map((block) =>
        block.id === id ? { ...block, ...patch } : block,
      ),
    );
  };
  const beginGesture = (event, block, kind) => {
    if (testMode) return;
    event.stopPropagation();
    event.currentTarget.setPointerCapture(event.pointerId);
    onSelectBlock(block.id);
    setGesture({
      id: block.id,
      kind,
      startX: event.clientX,
      startY: event.clientY,
      frame: { ...block },
    });
  };
  const moveGesture = (event) => {
    if (!gesture || !surfaceRef.current) return;
    const rect = surfaceRef.current.getBoundingClientRect();
    const dx = snap(((event.clientX - gesture.startX) * 1000) / rect.width);
    const dy = snap(((event.clientY - gesture.startY) * 1000) / rect.height);
    if (gesture.kind === "resize") {
      updateBlock(gesture.id, {
        width: clamp(
          snap(gesture.frame.width + dx),
          80,
          1000 - gesture.frame.x,
        ),
        height: clamp(
          snap(gesture.frame.height + dy),
          60,
          1000 - gesture.frame.y,
        ),
      });
    } else {
      updateBlock(gesture.id, {
        x: clamp(snap(gesture.frame.x + dx), 0, 1000 - gesture.frame.width),
        y: clamp(snap(gesture.frame.y + dy), 0, 1000 - gesture.frame.height),
      });
    }
  };
  const overlaps = theme.homeCanvasBlocks.reduce(
    (count, block, index, blocks) =>
      count +
      blocks
        .slice(index + 1)
        .filter(
          (other) =>
            block.x < other.x + other.width &&
            block.x + block.width > other.x &&
            block.y < other.y + other.height &&
            block.y + block.height > other.y,
        ).length,
    0,
  );
  const undersizedBlocks = theme.homeCanvasBlocks.filter((block) => {
    const minimums = {
      recentBooks:
        theme.homeLayout === "dashboard"
          ? [400, 220]
          : theme.homeLayout === "spotlight"
            ? [280, 300]
            : [360, 240],
      bookProgress: [220, 80],
      bookStats: [300, 100],
      globalStats: [300, 100],
      quickActions: [300, 120],
      menuTrigger: [180, 70],
    };
    const [minimumWidth, minimumHeight] = minimums[block.type] || [80, 60];
    return block.width < minimumWidth || block.height < minimumHeight;
  }).length;
  const moveFocus = (direction) => {
    if (menuOpen) {
      setPanelIndex((current) => {
        const columns = theme.homePanelColumns;
        if (direction < 0 && current % columns > 0) return current - 1;
        if (
          direction > 0 &&
          current % columns < columns - 1 &&
          current + 1 < theme.homeActionOrder.length
        )
          return current + 1;
        return current;
      });
      return;
    }
    if (interactiveBlocks.length === 0) return;
    const current = interactiveBlocks.findIndex(
      (block) => block.id === focusedBlockId,
    );
    const next =
      (Math.max(0, current) + direction + interactiveBlocks.length) %
      interactiveBlocks.length;
    setFocusedBlockId(interactiveBlocks[next].id);
  };
  const activateFocused = () => {
    if (menuOpen) return;
    const focused = interactiveBlocks.find(
      (block) => block.id === focusedBlockId,
    );
    if (focused?.type === "menuTrigger") {
      setMenuOpen(true);
      setPanelIndex(0);
    }
  };
  const handleTestKeyDown = (event) => {
    if (!testMode) return;
    if (event.key === "Escape") {
      setMenuOpen(false);
      return;
    }
    if (event.key === "Enter") {
      event.preventDefault();
      activateFocused();
      return;
    }
    if (menuOpen) {
      const columns = theme.homePanelColumns;
      if (event.key === "ArrowLeft") moveFocus(-1);
      if (event.key === "ArrowRight") moveFocus(1);
      if (event.key === "ArrowUp")
        setPanelIndex((current) =>
          current >= columns
            ? current - columns
            : theme.homeActionOrder.length - 1,
        );
      if (event.key === "ArrowDown")
        setPanelIndex((current) =>
          current + columns < theme.homeActionOrder.length
            ? current + columns
            : 0,
        );
      if (event.key.startsWith("Arrow")) event.preventDefault();
      return;
    }
    if (event.key === "ArrowUp") moveFocus(-1);
    if (event.key === "ArrowDown") moveFocus(1);
    const focused = interactiveBlocks.find(
      (block) => block.id === focusedBlockId,
    );
    if (
      (event.key === "ArrowLeft" || event.key === "ArrowRight") &&
      focused?.type === "recentBooks"
    ) {
      const direction = event.key === "ArrowLeft" ? -1 : 1;
      setSelectedBookIndex(
        (current) =>
          (current + direction + theme.homeRecentBooks) % theme.homeRecentBooks,
      );
    }
    if (
      (event.key === "ArrowLeft" || event.key === "ArrowRight") &&
      focused?.type === "quickActions" &&
      visibleQuickActions.length > 0
    ) {
      const direction = event.key === "ArrowLeft" ? -1 : 1;
      setQuickActionIndex(
        (current) =>
          (current + direction + visibleQuickActions.length) %
          visibleQuickActions.length,
      );
    }
    if (event.key.startsWith("Arrow")) event.preventDefault();
  };

  return (
    <div className="canvas-shell">
      <div
        className={`canvas-device-frame frame-${deviceProfile.label.toLowerCase()}`}
      >
        {deviceProfile.label === "X3" && (
          <>
            <span className="x3-side-control x3-side-left">Up</span>
            <span className="x3-side-control x3-side-right">Down</span>
          </>
        )}
        <div
          className={`canvas-device device-${deviceProfile.label.toLowerCase()}`}
          tabIndex={testMode ? 0 : -1}
          aria-label={
            testMode
              ? "Interactive device preview. Use arrow keys, Enter and Escape."
              : "Device design preview"
          }
          onKeyDown={handleTestKeyDown}
          style={{
            aspectRatio: `${deviceProfile.width} / ${deviceProfile.height}`,
          }}
        >
          <div
            className="canvas-header"
            style={{ height: `${headerPercent}%` }}
          >
            <b>Library</b>
            <span>10:42&nbsp; 82%</span>
          </div>
          <div
            className="canvas-surface"
            ref={surfaceRef}
            style={{ top: `${headerPercent}%`, bottom: `${hintsPercent}%` }}
            onPointerDown={() => !testMode && onSelectBlock(null)}
          >
            {menuOpen && testMode ? (
              <div
                className="canvas-menu-panel"
                style={{
                  gridTemplateColumns: `repeat(${theme.homePanelColumns}, 1fr)`,
                  gridAutoRows: `${Math.max(24, theme.rowHeight * 0.5)}px`,
                  gap: `${Math.max(2, theme.gap * 0.5)}px`,
                  inset: `${(theme.verticalSpacing / deviceProfile.height) * 100}% ${(theme.sidePadding / deviceProfile.width) * 100}%`,
                }}
              >
                {theme.homeActionOrder.map((id, index) => (
                  <button
                    className={panelIndex === index ? "active" : ""}
                    type="button"
                    key={id}
                    onClick={() => setPanelIndex(index)}
                  >
                    {homeActionCatalog.find(([action]) => action === id)?.[1]}
                  </button>
                ))}
              </div>
            ) : (
              theme.homeCanvasBlocks.map((block) => (
                <div
                  className={`canvas-block ${testMode ? "test" : ""} ${testMode && focusedBlockId === block.id ? "focused" : ""} ${selectedBlock === block.id && !testMode ? "selected" : ""}`}
                  key={block.id}
                  style={{
                    left: `${block.x / 10}%`,
                    top: `${block.y / 10}%`,
                    width: `${block.width / 10}%`,
                    height: `${block.height / 10}%`,
                    borderRadius: theme.cornerRadius / 2,
                  }}
                  onPointerDown={(event) => beginGesture(event, block, "move")}
                  onPointerMove={moveGesture}
                  onPointerUp={() => setGesture(null)}
                  onClick={() => {
                    if (testMode) setFocusedBlockId(block.id);
                    if (
                      testMode &&
                      block.type === "recentBooks" &&
                      theme.homeRecentBooks > 1
                    ) {
                      setSelectedBookIndex(
                        (current) => (current + 1) % theme.homeRecentBooks,
                      );
                    }
                    if (testMode && block.type === "menuTrigger") {
                      setMenuOpen(true);
                      setPanelIndex(0);
                    }
                    if (
                      testMode &&
                      block.type === "quickActions" &&
                      visibleQuickActions.length > 0
                    ) {
                      setQuickActionIndex(
                        (current) => (current + 1) % visibleQuickActions.length,
                      );
                    }
                  }}
                >
                  <CanvasBlockContent
                    block={block}
                    theme={theme}
                    testMode={testMode}
                    focused={focusedBlockId === block.id}
                    quickActionIndex={quickActionIndex}
                    onOpenMenu={() => setMenuOpen(true)}
                    selectedBookIndex={selectedBookIndex}
                  />
                  {!testMode && (
                    <>
                      <button
                        type="button"
                        className="block-remove"
                        aria-label={`Remove ${canvasBlockCatalog.find(([type]) => type === block.type)?.[1] || "block"}`}
                        title="Remove from Home"
                        onPointerDown={(event) => event.stopPropagation()}
                        onClick={(event) => {
                          event.stopPropagation();
                          onRemoveBlock(block.id);
                        }}
                      >
                        <Trash2 size={11} />
                      </button>
                      <i
                        className="resize-handle"
                        onPointerDown={(event) =>
                          beginGesture(event, block, "resize")
                        }
                      />
                    </>
                  )}
                </div>
              ))
            )}
          </div>
          <div className="canvas-hints" style={{ height: `${hintsPercent}%` }}>
            <button
              type="button"
              disabled={!testMode}
              onClick={() => {
                setMenuOpen((current) => !current);
                setPanelIndex(0);
              }}
            >
              {menuOpen ? "Back" : "Menu"}
            </button>
            <button
              type="button"
              disabled={!testMode}
              onClick={activateFocused}
            >
              Select
            </button>
            <button
              type="button"
              disabled={!testMode}
              onClick={() => moveFocus(-1)}
            >
              {menuOpen ? "Left" : "Up"}
            </button>
            <button
              type="button"
              disabled={!testMode}
              onClick={() => moveFocus(1)}
            >
              {menuOpen ? "Right" : "Down"}
            </button>
          </div>
        </div>
      </div>
      <Badge>
        {deviceProfile.label} · {deviceProfile.width}×{deviceProfile.height}
      </Badge>
      {!testMode && (
        <p
          className={
            overlaps || undersizedBlocks ? "canvas-warning" : "canvas-status"
          }
        >
          {overlaps
            ? `${overlaps} overlapping block pair${overlaps > 1 ? "s" : ""}`
            : undersizedBlocks
              ? `${undersizedBlocks} block${undersizedBlocks > 1 ? "s are" : " is"} too small for reliable content`
              : "Canvas valid · content fits · snap 10"}
        </p>
      )}
    </div>
  );
}

function Preview({
  theme,
  onChange,
  selectedBlock,
  onSelectBlock,
  onRemoveBlock,
  testMode,
  deviceProfile,
  previewTab,
  onPreviewTabChange,
}) {
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
  const previewScreenStyle = {
    aspectRatio: `${deviceProfile.width} / ${deviceProfile.height}`,
  };
  return (
    <Tabs value={previewTab} onValueChange={onPreviewTabChange}>
      <TabsList className="preview-tabs">
        <TabsTrigger value="home">Home</TabsTrigger>
        <TabsTrigger value="navigation">Navigation</TabsTrigger>
        <TabsTrigger value="dialogs">Dialogs</TabsTrigger>
        <TabsTrigger value="reader">Reader</TabsTrigger>
      </TabsList>
      <TabsContent value="home">
        <HomeCanvasEditor
          theme={theme}
          onChange={onChange}
          selectedBlock={selectedBlock}
          onSelectBlock={onSelectBlock}
          onRemoveBlock={onRemoveBlock}
          testMode={testMode}
          deviceProfile={deviceProfile}
        />
      </TabsContent>
      <TabsContent value="navigation">
        <div className="component-screen" style={previewScreenStyle}>
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
        <div
          className="component-screen dialog-screen"
          style={previewScreenStyle}
        >
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
        <div className="reader-screen" style={previewScreenStyle}>
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
  const [theme, setTheme] = useState(getInitialTheme),
    [message, setMessage] = useState(""),
    [selectedBlock, setSelectedBlock] = useState("recents"),
    [testMode, setTestMode] = useState(false),
    [deviceId, setDeviceId] = useState("x4"),
    [activeSection, setActiveSection] = useState("overview"),
    [previewTab, setPreviewTab] = useState("home"),
    [draftSaved, setDraftSaved] = useState(true),
    [busyAction, setBusyAction] = useState(null);
  const importInputRef = useRef(null);
  const deviceProfile = deviceProfiles[deviceId];
  const update = (field, value) => {
    setDraftSaved(false);
    setTheme((current) => ({ ...current, [field]: value }));
  };
  useEffect(() => {
    const timeout = window.setTimeout(() => {
      try {
        window.localStorage.setItem(
          "crossink-theme-studio-draft-v4",
          JSON.stringify(theme),
        );
        setDraftSaved(true);
      } catch {
        setDraftSaved(false);
      }
    }, 300);
    return () => window.clearTimeout(timeout);
  }, [theme]);
  const activeCanvasBlock =
    theme.homeCanvasBlocks.find((block) => block.id === selectedBlock) || null;
  const updateCanvasBlock = (patch) => {
    if (!activeCanvasBlock) return;
    update(
      "homeCanvasBlocks",
      theme.homeCanvasBlocks.map((block) =>
        block.id === activeCanvasBlock.id ? { ...block, ...patch } : block,
      ),
    );
  };
  const addCanvasBlock = (type) => {
    if (
      theme.homeCanvasBlocks.length >= 8 ||
      theme.homeCanvasBlocks.some((block) => block.type === type)
    )
      return;
    const [width, height] = suggestedCanvasSize(type);
    const placement = findCanvasPlacement(
      theme.homeCanvasBlocks,
      width,
      height,
    );
    const next = {
      id: `${type}-${Date.now().toString(36)}`,
      type,
      ...(type === "recentBooks" ? { variant: "cards" } : {}),
      ...placement,
      width,
      height,
    };
    update("homeCanvasBlocks", [...theme.homeCanvasBlocks, next]);
    setSelectedBlock(next.id);
  };
  const autoArrangeCanvas = () => {
    update("homeCanvasBlocks", arrangeCanvasBlocks(theme.homeCanvasBlocks));
    setMessage("Home blocks arranged into a non-overlapping starting layout.");
  };
  const removeCanvasBlock = (id = activeCanvasBlock?.id) => {
    if (!id) return;
    update(
      "homeCanvasBlocks",
      theme.homeCanvasBlocks.filter((block) => block.id !== id),
    );
    if (selectedBlock === id) setSelectedBlock(null);
  };
  const clearCanvas = () => {
    update("homeCanvasBlocks", []);
    setSelectedBlock(null);
    setMessage(
      "Home canvas cleared. The first device button still opens the action menu.",
    );
  };
  const moveHomeAction = (id, direction) => {
    const order = [...theme.homeActionOrder];
    const index = order.indexOf(id);
    const target = index + direction;
    if (index < 0 || target < 0 || target >= order.length) return;
    [order[index], order[target]] = [order[target], order[index]];
    update("homeActionOrder", order);
  };
  const togglePinnedAction = (id) => {
    const pinned = theme.homePinnedActions.includes(id)
      ? theme.homePinnedActions.filter((item) => item !== id)
      : theme.homePinnedActions.length < 3
        ? [...theme.homePinnedActions, id]
        : theme.homePinnedActions;
    update("homePinnedActions", pinned);
  };
  const setHomeMenuPresentation = (presentation) => {
    setTheme((current) => {
      return {
        ...current,
        homeMenuPresentation: presentation,
      };
    });
  };
  const settingsOrder = useMemo(
    () =>
      theme.settingsOrder
        .split(/[\s,]+/)
        .map((item) => item.trim())
        .filter(Boolean),
    [theme.settingsOrder],
  );
  const validation = useMemo(() => {
    const blockTypes = theme.homeCanvasBlocks.map((block) => block.type);
    const validBlocks =
      theme.homeCanvasBlocks.length <= 8 &&
      new Set(blockTypes).size === blockTypes.length &&
      theme.homeCanvasBlocks.every(
        (block) =>
          block.x >= 0 &&
          block.y >= 0 &&
          block.width >= 1 &&
          block.height >= 1 &&
          block.x + block.width <= 1000 &&
          block.y + block.height <= 1000,
      );
    const issues = [];
    if (!theme.name.trim()) issues.push("Add a theme name");
    if (!/^[a-z0-9-]{1,32}$/.test(theme.id))
      issues.push(
        "Use a lowercase identifier with letters, numbers or hyphens",
      );
    if (
      settingsOrder.length > 8 ||
      !settingsOrder.every((item) => /^[A-Za-z0-9_-]{1,48}$/.test(item)) ||
      new Set(settingsOrder).size !== settingsOrder.length
    )
      issues.push("Fix the preferred settings order");
    if (!validBlocks) issues.push("Keep Home blocks inside the canvas");
    if (theme.homePinnedActions.length > 3)
      issues.push("Pin no more than 3 Home actions");
    return {
      issues,
      name: Boolean(theme.name.trim()),
      id: /^[a-z0-9-]{1,32}$/.test(theme.id),
      settings: !issues.includes("Fix the preferred settings order"),
    };
  }, [settingsOrder, theme]);
  const valid = validation.issues.length === 0;
  const importTheme = async (event) => {
    const file = event.target.files?.[0];
    if (!file) return;
    setBusyAction("open");
    try {
      let manifest;
      if (file.name.toLowerCase().endsWith(".json")) {
        manifest = JSON.parse(await file.text());
      } else {
        const archive = await JSZip.loadAsync(file);
        const manifestFile = archive.file("theme.json");
        if (!manifestFile) throw new Error("theme.json is missing");
        manifest = JSON.parse(await manifestFile.async("text"));
      }
      if (!manifest || typeof manifest !== "object")
        throw new Error("Theme manifest is invalid");
      setTheme(themeFromManifest(manifest));
      setSelectedBlock(null);
      setTestMode(false);
      setActiveSection("overview");
      setPreviewTab("home");
      setMessage(`Opened ${file.name}. Review the preview before exporting.`);
    } catch (error) {
      setMessage(`Could not open this theme: ${error.message}`);
    } finally {
      setBusyAction(null);
      event.target.value = "";
    }
  };
  const exportTheme = async () => {
    if (!valid) {
      setMessage("Add a name and a valid lowercase identifier.");
      return;
    }
    setBusyAction("export");
    try {
      const manifest = {
        schemaVersion: 4,
        engine: "declarative",
        id: theme.id,
        name: theme.name.trim(),
        home: {
          layoutEngine: "canvas",
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
          blocks: theme.homeCanvasBlocks.map(
            ({ type, variant, x, y, width, height }) => ({
              type,
              ...(type === "recentBooks"
                ? { variant: variant || "cards" }
                : {}),
              frame: { x, y, width, height },
            }),
          ),
          actions: {
            presentation: theme.homeMenuPresentation,
            pinned: theme.homePinnedActions,
            order: theme.homeActionOrder,
            panel: { columns: theme.homePanelColumns },
          },
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
            columns:
              theme.settingsLayout === "cards" ? 1 : theme.settingsColumns,
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
    } catch (error) {
      setMessage(`Could not export this theme: ${error.message}`);
    } finally {
      setBusyAction(null);
    }
  };
  const currentSection = studioSections.find(
    (section) => section.id === activeSection,
  );
  return (
    <main id="main-content">
      <a className="skip-link" href="#editor-heading">
        Skip to editor
      </a>
      <header className="hero">
        <div className="studio-brand">
          <span className="brand-mark" aria-hidden="true">
            <LayoutTemplate size={20} />
          </span>
          <div>
            <span>CrossInk</span>
            <h1>Theme Studio</h1>
          </div>
          <Badge>v4</Badge>
        </div>
        <div className="save-state" role="status">
          {draftSaved ? <Check size={14} /> : <span className="save-dot" />}
          {draftSaved ? "Draft saved locally" : "Saving draft…"}
        </div>
        <div className="hero-actions">
          <input
            ref={importInputRef}
            className="visually-hidden"
            type="file"
            accept=".cptheme,.json,application/json,application/zip"
            onChange={importTheme}
          />
          <Button
            variant="outline"
            disabled={Boolean(busyAction)}
            onClick={() => importInputRef.current?.click()}
          >
            <Upload size={16} />
            {busyAction === "open" ? "Opening…" : "Open theme"}
          </Button>
          <Button
            variant="ghost"
            onClick={() => {
              if (!window.confirm("Restore every setting to the defaults?"))
                return;
              setTheme(defaults);
              setSelectedBlock("recents");
              setTestMode(false);
              setActiveSection("overview");
              setPreviewTab("home");
              setMessage("Defaults restored.");
            }}
          >
            <RotateCcw size={16} /> Reset
          </Button>
          <Button
            onClick={exportTheme}
            disabled={!valid || Boolean(busyAction)}
          >
            <Download size={16} />
            {busyAction === "export" ? "Exporting…" : "Export"}
          </Button>
        </div>
      </header>
      <div className="workspace">
        <nav className="studio-nav" aria-label="Theme editor sections">
          <p>Customize</p>
          {studioSections.map((section, index) => {
            const Icon = section.icon;
            const active = activeSection === section.id;
            return (
              <button
                key={section.id}
                type="button"
                className={active ? "active" : ""}
                aria-current={active ? "step" : undefined}
                onClick={() => {
                  setActiveSection(section.id);
                  if (section.id === "home") setPreviewTab("home");
                  if (section.id === "components") setPreviewTab("navigation");
                  if (section.id === "settings") setPreviewTab("navigation");
                }}
              >
                <span className="nav-index">{index + 1}</span>
                <Icon size={18} />
                <span>
                  <b>{section.label}</b>
                  <small>{section.description}</small>
                </span>
                {section.id === "overview" &&
                  validation.name &&
                  validation.id && (
                    <CheckCircle2 className="nav-check" size={16} />
                  )}
              </button>
            );
          })}
          <div className={valid ? "readiness ready" : "readiness"}>
            {valid ? <CheckCircle2 size={18} /> : <AlertCircle size={18} />}
            <span>
              <b>{valid ? "Ready to export" : "Needs attention"}</b>
              <small>
                {valid
                  ? "Theme manifest is valid"
                  : `${validation.issues.length} ${validation.issues.length === 1 ? "issue" : "issues"}`}
              </small>
            </span>
          </div>
        </nav>
        <Card className="editor-card">
          <CardHeader>
            <div>
              <span className="eyebrow">
                Step {studioSections.indexOf(currentSection) + 1} of{" "}
                {studioSections.length}
              </span>
              <CardTitle id="editor-heading">{currentSection.label}</CardTitle>
              <CardDescription>{currentSection.description}</CardDescription>
            </div>
          </CardHeader>
          <CardContent>
            <div
              className={`overview-editor ${activeSection === "overview" ? "" : "studio-section-hidden"}`}
            >
              <div className="section-intro">
                <span className="section-icon">
                  <Palette size={20} />
                </span>
                <div>
                  <h3>Start with a recognizable name</h3>
                  <p>
                    The identifier becomes the file name and the permanent key
                    used by the firmware.
                  </p>
                </div>
              </div>
              <div className="metadata">
                <div>
                  <Label htmlFor="name">Theme name</Label>
                  <Input
                    id="name"
                    value={theme.name}
                    maxLength={48}
                    aria-invalid={!validation.name}
                    onChange={(e) => update("name", e.target.value)}
                  />
                  <small>Shown in the device theme picker.</small>
                </div>
                <div>
                  <Label htmlFor="id">Identifier</Label>
                  <Input
                    id="id"
                    value={theme.id}
                    maxLength={32}
                    aria-invalid={!validation.id}
                    onChange={(e) =>
                      update(
                        "id",
                        e.target.value.toLowerCase().replace(/[^a-z0-9-]/g, ""),
                      )
                    }
                  />
                  <small>Lowercase letters, numbers and hyphens only.</small>
                </div>
              </div>
              <div className="file-panel">
                <div>
                  <Upload size={20} />
                  <span>
                    <b>Continue an existing theme</b>
                    <small>
                      Open a .cptheme package or its theme.json manifest.
                    </small>
                  </span>
                </div>
                <Button
                  variant="outline"
                  disabled={Boolean(busyAction)}
                  onClick={() => importInputRef.current?.click()}
                >
                  {busyAction === "open" ? "Opening…" : "Choose file"}
                </Button>
              </div>
              <div
                className={
                  valid ? "validation-panel valid" : "validation-panel"
                }
              >
                <div>
                  {valid ? (
                    <CheckCircle2 size={20} />
                  ) : (
                    <AlertCircle size={20} />
                  )}
                  <span>
                    <b>
                      {valid ? "Everything looks good" : "Before you export"}
                    </b>
                    <small>
                      {valid
                        ? "The package can be installed on CrossInk."
                        : "Complete the items below."}
                    </small>
                  </span>
                </div>
                {!valid && (
                  <ul>
                    {validation.issues.map((issue) => (
                      <li key={issue}>{issue}</li>
                    ))}
                  </ul>
                )}
              </div>
            </div>
            <Accordion
              type="multiple"
              defaultValue={["settings", "home", "menu"]}
              className={`controls ${activeSection === "overview" ? "studio-section-hidden" : ""}`}
            >
              <AccordionItem
                value="home"
                className={
                  activeSection === "home" ? "" : "studio-section-hidden"
                }
              >
                <AccordionTrigger>Home composition</AccordionTrigger>
                <AccordionContent>
                  <div className="canvas-controls">
                    <div className="palette-heading">
                      <div>
                        <Label>Home blocks</Label>
                        <small>
                          Add only what the Home needs. Decorative data is
                          supplied by the firmware.
                        </small>
                      </div>
                      <div className="palette-actions">
                        <Button
                          type="button"
                          size="sm"
                          variant="outline"
                          disabled={theme.homeCanvasBlocks.length === 0}
                          onClick={autoArrangeCanvas}
                        >
                          <LayoutTemplate size={14} /> Auto arrange
                        </Button>
                        <Button
                          type="button"
                          size="sm"
                          variant="ghost"
                          disabled={theme.homeCanvasBlocks.length === 0}
                          onClick={clearCanvas}
                        >
                          Clear
                        </Button>
                      </div>
                    </div>
                    <div>
                      <div className="block-palette">
                        {canvasBlockCatalog.map(([type, label]) => {
                          const block = theme.homeCanvasBlocks.find(
                            (item) => item.type === type,
                          );
                          return (
                            <div
                              className={block ? "block-added" : ""}
                              key={type}
                            >
                              <span>
                                <b>{label}</b>
                                <small>{canvasBlockDetails[type]}</small>
                              </span>
                              {block ? (
                                <Button
                                  type="button"
                                  size="sm"
                                  variant="ghost"
                                  onClick={() => removeCanvasBlock(block.id)}
                                >
                                  <Trash2 size={14} /> Remove
                                </Button>
                              ) : (
                                <Button
                                  type="button"
                                  size="sm"
                                  variant="outline"
                                  disabled={theme.homeCanvasBlocks.length >= 8}
                                  onClick={() => addCanvasBlock(type)}
                                >
                                  <Plus size={14} /> Add
                                </Button>
                              )}
                            </div>
                          );
                        })}
                      </div>
                    </div>
                    {activeCanvasBlock && (
                      <div className="block-inspector">
                        <div className="inspector-heading">
                          <div>
                            <Label>Selected block</Label>
                            <b>
                              {
                                canvasBlockCatalog.find(
                                  ([type]) => type === activeCanvasBlock.type,
                                )?.[1]
                              }
                            </b>
                          </div>
                          <Button
                            type="button"
                            size="sm"
                            variant="ghost"
                            onClick={() => removeCanvasBlock()}
                          >
                            <Trash2 size={14} /> Remove from Home
                          </Button>
                        </div>
                        {activeCanvasBlock.type === "recentBooks" && (
                          <div className="appearance-picker">
                            <Label>Book block appearance</Label>
                            <div className="layout-picker">
                              {[
                                ["cards", "Cards"],
                                ["plain", "Unframed"],
                              ].map(([variant, label]) => (
                                <Button
                                  key={variant}
                                  type="button"
                                  size="sm"
                                  variant={
                                    (activeCanvasBlock.variant || "cards") ===
                                    variant
                                      ? "default"
                                      : "outline"
                                  }
                                  onClick={() => updateCanvasBlock({ variant })}
                                >
                                  {label}
                                </Button>
                              ))}
                            </div>
                          </div>
                        )}
                        <div className="frame-inputs">
                          {["x", "y", "width", "height"].map((field) => (
                            <Label key={field}>
                              {field}
                              <Input
                                type="number"
                                min={0}
                                max={
                                  field === "x"
                                    ? 1000 - activeCanvasBlock.width
                                    : field === "y"
                                      ? 1000 - activeCanvasBlock.height
                                      : field === "width"
                                        ? 1000 - activeCanvasBlock.x
                                        : 1000 - activeCanvasBlock.y
                                }
                                value={activeCanvasBlock[field]}
                                onChange={(event) => {
                                  const minimum =
                                    field === "width"
                                      ? 80
                                      : field === "height"
                                        ? 60
                                        : 0;
                                  const maximum =
                                    field === "x"
                                      ? 1000 - activeCanvasBlock.width
                                      : field === "y"
                                        ? 1000 - activeCanvasBlock.height
                                        : field === "width"
                                          ? 1000 - activeCanvasBlock.x
                                          : 1000 - activeCanvasBlock.y;
                                  const value = clamp(
                                    Number(event.target.value) || 0,
                                    minimum,
                                    maximum,
                                  );
                                  updateCanvasBlock({ [field]: value });
                                }}
                              />
                            </Label>
                          ))}
                        </div>
                      </div>
                    )}
                  </div>
                  <div className="home-menu-editor">
                    <div className="inspector-heading">
                      <div>
                        <Label>Home actions</Label>
                        <b>Presentation and order</b>
                      </div>
                    </div>
                    <div className="layout-picker">
                      {[
                        ["inline", "Inline", "All actions on Home"],
                        ["panel", "Panel", "Actions behind Menu"],
                        ["hybrid", "Hybrid", "Pinned + full panel"],
                      ].map(([presentation, label, description]) => (
                        <button
                          className={`choice-card ${theme.homeMenuPresentation === presentation ? "active" : ""}`}
                          key={presentation}
                          type="button"
                          onClick={() => setHomeMenuPresentation(presentation)}
                        >
                          <b>{label}</b>
                          <small>{description}</small>
                        </button>
                      ))}
                    </div>
                    <p className="component-note">
                      {theme.homeMenuPresentation === "inline"
                        ? "Add Quick actions to show the complete action list on Home."
                        : theme.homeMenuPresentation === "hybrid"
                          ? "Use Quick actions for pinned shortcuts and Menu button for the full panel."
                          : "The hardware Menu key always opens the panel; a Menu button block is optional."}
                    </p>
                    <div className="action-order">
                      {theme.homeActionOrder.map((id, index) => {
                        const label = homeActionCatalog.find(
                          ([action]) => action === id,
                        )?.[1];
                        const pinned = theme.homePinnedActions.includes(id);
                        const inline = theme.homeMenuPresentation === "inline";
                        return (
                          <div key={id}>
                            <span>
                              <b>{label}</b>
                              <small>
                                {inline
                                  ? "Shown by Quick actions"
                                  : pinned
                                    ? "Pinned on Home"
                                    : "Inside menu"}
                              </small>
                            </span>
                            <Button
                              type="button"
                              size="sm"
                              variant={pinned ? "default" : "outline"}
                              disabled={inline}
                              onClick={() => togglePinnedAction(id)}
                            >
                              {pinned ? "Pinned" : "Pin"}
                            </Button>
                            <Button
                              type="button"
                              size="sm"
                              variant="ghost"
                              disabled={index === 0}
                              aria-label={`Move ${label} up`}
                              title="Move up"
                              onClick={() => moveHomeAction(id, -1)}
                            >
                              <ChevronUp size={16} />
                            </Button>
                            <Button
                              type="button"
                              size="sm"
                              variant="ghost"
                              disabled={
                                index === theme.homeActionOrder.length - 1
                              }
                              aria-label={`Move ${label} down`}
                              title="Move down"
                              onClick={() => moveHomeAction(id, 1)}
                            >
                              <ChevronDown size={16} />
                            </Button>
                          </div>
                        );
                      })}
                    </div>
                    {theme.homeMenuPresentation !== "inline" && (
                      <RangeField
                        field="homePanelColumns"
                        label="Panel columns"
                        min={1}
                        max={3}
                        value={theme.homePanelColumns}
                        onChange={update}
                      />
                    )}
                    <p className="canvas-status">
                      The first device button always opens this menu. Menu and
                      Quick actions blocks are optional Home shortcuts.
                    </p>
                  </div>
                  {theme.homeCanvasBlocks.some(
                    (block) => block.type === "recentBooks",
                  ) && (
                    <div className="recent-books-editor">
                      <div className="inspector-heading">
                        <div>
                          <Label>Recent books</Label>
                          <b>Content and composition</b>
                        </div>
                      </div>
                      <div
                        className="layout-picker"
                        role="group"
                        aria-label="Recent books layout"
                      >
                        {homeLayoutCatalog.map((layout) => (
                          <button
                            className={`choice-card ${theme.homeLayout === layout.id ? "active" : ""}`}
                            key={layout.id}
                            type="button"
                            onClick={() => update("homeLayout", layout.id)}
                          >
                            <b>{layout.label}</b>
                            <small>{layout.description}</small>
                          </button>
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
                          field="homeBookGap"
                          label="Book gap"
                          min={0}
                          max={40}
                          value={theme.homeBookGap}
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
                      <p className="canvas-status">
                        In Test mode, click the recent-books block to navigate
                        between books.
                      </p>
                    </div>
                  )}
                </AccordionContent>
              </AccordionItem>
              <AccordionItem
                value="settings"
                className={
                  activeSection === "settings" ? "" : "studio-section-hidden"
                }
              >
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
                      aria-invalid={!validation.settings}
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
                <AccordionItem
                  key={group.value}
                  value={group.value}
                  className={
                    activeSection === "components"
                      ? ""
                      : "studio-section-hidden"
                  }
                >
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
                What you see is normalized to the selected device geometry.
              </CardDescription>
            </div>
            <div className="preview-actions">
              <div className="device-picker" role="group" aria-label="Device">
                {Object.entries(deviceProfiles).map(([id, profile]) => (
                  <Button
                    key={id}
                    type="button"
                    size="sm"
                    variant={deviceId === id ? "default" : "outline"}
                    onClick={() => setDeviceId(id)}
                  >
                    {profile.label}
                  </Button>
                ))}
              </div>
              <Button
                type="button"
                size="sm"
                variant={testMode ? "outline" : "default"}
                onClick={() => setTestMode(false)}
              >
                Design
              </Button>
              <Button
                type="button"
                size="sm"
                variant={testMode ? "default" : "outline"}
                onClick={() => {
                  setTestMode(true);
                  setPreviewTab("home");
                }}
              >
                Test
              </Button>
            </div>
          </CardHeader>
          <CardContent>
            <div className="preview-context">
              <Layers3 size={16} />
              <span>
                <b>{testMode ? "Test mode" : "Design mode"}</b>
                <small>
                  {testMode
                    ? "Use the device buttons or focus the screen and press arrows, Enter and Escape."
                    : previewTab === "home"
                      ? "Select, drag and resize Home blocks."
                      : "Adjust controls and compare the component sample."}
                </small>
              </span>
            </div>
            <Preview
              theme={theme}
              onChange={update}
              selectedBlock={selectedBlock}
              onSelectBlock={setSelectedBlock}
              onRemoveBlock={removeCanvasBlock}
              testMode={testMode}
              deviceProfile={deviceProfile}
              previewTab={previewTab}
              onPreviewTabChange={setPreviewTab}
            />
          </CardContent>
        </Card>
      </div>
    </main>
  );
}
