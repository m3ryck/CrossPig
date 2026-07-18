#pragma once

#include <array>
#include <string>
#include <vector>

#include <I18n.h>

#include "activities/Activity.h"
#include "CrossPointSettings.h"
#include "components/CustomThemeRegistry.h"
#include "util/ButtonNavigator.h"

// Edits the eight renderers that make up the persisted Custom UI theme.
// Theme packages are selection sources only: their resolved built-in renderer
// is copied into settings, so removing a package cannot break the selection.
class ThemeComposerActivity final : public Activity {
 public:
  ThemeComposerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

 void onEnter() override;
 void loop() override;
 void render(RenderLock&&) override;

  // Kept public for the small static component table in the implementation.
  enum class Component : uint8_t { Home, Header, List, Menu, Popup, Input, Hints, Status, Count };

 private:

  static uint8_t CrossPointSettings::*componentSetting(Component component);
  static StrId componentName(Component component);
  static std::string builtInThemeName(uint8_t theme);

  void openComponentPicker(Component component);
  uint8_t resolveSource(Component component, size_t sourceIndex) const;
  void rebuildSources();

  ButtonNavigator buttonNavigator_;
  int selectedIndex_ = 0;
  std::vector<std::string> sourceNames_;
};
