#include "ThemeComposerActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>

#include <array>
#include <cstring>
#include <memory>
#include <utility>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "activities/util/OptionSelectionActivity.h"
#include "components/CustomThemeRegistry.h"
#include "Memory.h"
#include "components/UITheme.h"

namespace {
constexpr std::array<uint8_t, 7> kBuiltInThemes = {
    CrossPointSettings::CLASSIC,      CrossPointSettings::MINIMAL, CrossPointSettings::DASHBOARD,
    CrossPointSettings::LYRA,         CrossPointSettings::LYRA_3_COVERS,
    CrossPointSettings::LYRA_CAROUSEL, CrossPointSettings::ROUNDEDRAFF,
};

constexpr int kComponentCount = static_cast<int>(ThemeComposerActivity::Component::Count);
}  // namespace

ThemeComposerActivity::ThemeComposerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("ThemeComposer", renderer, mappedInput) {}

void ThemeComposerActivity::onEnter() {
  Activity::onEnter();
  CUSTOM_THEMES.discover();
  rebuildSources();
  requestUpdate();
}

void ThemeComposerActivity::rebuildSources() {
  sourceNames_.clear();
  sourceNames_.reserve(kBuiltInThemes.size() + CustomThemeRegistry::kMaxThemes);
  for (const uint8_t theme : kBuiltInThemes) sourceNames_.push_back(builtInThemeName(theme));
  for (const auto& theme : CUSTOM_THEMES.getThemes()) sourceNames_.push_back(theme.name);
}

StrId ThemeComposerActivity::componentName(const Component component) {
  static constexpr std::array<StrId, kComponentCount> names = {
      StrId::STR_THEME_COMPONENT_HOME,   StrId::STR_THEME_COMPONENT_HEADER,
      StrId::STR_THEME_COMPONENT_LIST,   StrId::STR_THEME_COMPONENT_MENU,
      StrId::STR_THEME_COMPONENT_POPUP,  StrId::STR_THEME_COMPONENT_INPUT,
      StrId::STR_THEME_COMPONENT_HINTS,  StrId::STR_THEME_COMPONENT_STATUS,
  };
  return names[static_cast<size_t>(component)];
}

uint8_t CrossPointSettings::*ThemeComposerActivity::componentSetting(const Component component) {
  static constexpr std::array<uint8_t CrossPointSettings::*, kComponentCount> settings = {
      &CrossPointSettings::customThemeHome,   &CrossPointSettings::customThemeHeader,
      &CrossPointSettings::customThemeList,   &CrossPointSettings::customThemeMenu,
      &CrossPointSettings::customThemePopup,  &CrossPointSettings::customThemeInput,
      &CrossPointSettings::customThemeHints,  &CrossPointSettings::customThemeStatus,
  };
  return settings[static_cast<size_t>(component)];
}

std::string ThemeComposerActivity::builtInThemeName(const uint8_t theme) {
  switch (theme) {
    case CrossPointSettings::CLASSIC:
      return tr(STR_THEME_CLASSIC);
    case CrossPointSettings::LYRA:
      return tr(STR_THEME_LYRA);
    case CrossPointSettings::LYRA_3_COVERS:
      return tr(STR_THEME_LYRA_EXTENDED);
    case CrossPointSettings::ROUNDEDRAFF:
      return tr(STR_THEME_ROUNDEDRAFF);
    case CrossPointSettings::LYRA_CAROUSEL:
      return tr(STR_THEME_LYRA_CAROUSEL);
    case CrossPointSettings::MINIMAL:
      return tr(STR_THEME_MINIMAL);
    case CrossPointSettings::DASHBOARD:
      return tr(STR_THEME_DASHBOARD);
    default:
      return tr(STR_THEME_LYRA);
  }
}

uint8_t ThemeComposerActivity::resolveSource(const Component component, const size_t sourceIndex) const {
  if (sourceIndex < kBuiltInThemes.size()) return kBuiltInThemes[sourceIndex];

  const size_t customIndex = sourceIndex - kBuiltInThemes.size();
  const auto& sources = CUSTOM_THEMES.getThemes();
  if (customIndex >= sources.size()) return CrossPointSettings::LYRA;
  const CustomThemeInfo& source = sources[customIndex];
  switch (component) {
    case Component::Home:
      return source.baseTheme;
    case Component::Header:
      return source.headerTheme;
    case Component::List:
      return source.listTheme;
    case Component::Menu:
      return source.menuTheme;
    case Component::Popup:
      return source.popupTheme;
    case Component::Input:
      return source.inputTheme;
    case Component::Hints:
      return source.hintsTheme;
    case Component::Status:
      return source.statusTheme;
    case Component::Count:
      return CrossPointSettings::LYRA;
  }
  return CrossPointSettings::LYRA;
}

void ThemeComposerActivity::openComponentPicker(const Component component) {
  const uint8_t currentTheme = SETTINGS.*(componentSetting(component));
  uint8_t selectedSource = 0;
  for (size_t i = 0; i < kBuiltInThemes.size(); ++i) {
    if (kBuiltInThemes[i] == currentTheme) {
      selectedSource = static_cast<uint8_t>(i);
      break;
    }
  }

  auto picker = makeUniqueNoThrow<OptionSelectionActivity>(renderer, mappedInput, "ThemeComponentSelect",
                                                           componentName(component), sourceNames_, selectedSource);
  if (!picker) {
    LOG_ERR("THEME", "Out of memory opening component picker");
    return;
  }
  startActivityForResult(
      std::move(picker),
      [this, component](const ActivityResult& result) {
        if (result.isCancelled) return;
        const auto* selection = std::get_if<OptionSelectionResult>(&result.data);
        if (!selection) return;
        SETTINGS.*(componentSetting(component)) = resolveSource(component, selection->index);
        SETTINGS.uiTheme = CrossPointSettings::CUSTOM_THEME;
        std::strncpy(SETTINGS.customThemeId, CustomThemeRegistry::kComposerId, sizeof(SETTINGS.customThemeId) - 1);
        SETTINGS.customThemeId[sizeof(SETTINGS.customThemeId) - 1] = '\0';
        SETTINGS.saveToFile();
        UITheme::getInstance().reload();
        requestUpdate();
      });
}

void ThemeComposerActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finishAfterBackPress();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    openComponentPicker(static_cast<Component>(selectedIndex_));
    return;
  }

  buttonNavigator_.onNextRelease([this] {
    selectedIndex_ = ButtonNavigator::nextIndex(selectedIndex_, kComponentCount);
    requestUpdate();
  });
  buttonNavigator_.onPreviousRelease([this] {
    selectedIndex_ = ButtonNavigator::previousIndex(selectedIndex_, kComponentCount);
    requestUpdate();
  });
}

void ThemeComposerActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const ThemeMetrics& metrics = UITheme::getInstance().getMetrics();
  const int width = renderer.getScreenWidth();
  const int height = renderer.getScreenHeight();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, width, metrics.headerHeight}, tr(STR_THEME_COMPONENTS), nullptr);

  const int top = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int listHeight = height - top - metrics.buttonHintsHeight - metrics.verticalSpacing;
  GUI.drawList(
      renderer, Rect{0, top, width, listHeight}, kComponentCount, selectedIndex_,
      [](const int index) { return std::string(I18N.get(componentName(static_cast<Component>(index)))); }, nullptr,
      nullptr,
      [](const int index) {
        return builtInThemeName(SETTINGS.*(componentSetting(static_cast<Component>(index))));
      }, true);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
