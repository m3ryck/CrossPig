#include "BookStoreActivity.h"

#include <I18n.h>
#include <Logging.h>

#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"

void BookStoreActivity::onEnter() {
  Activity::onEnter();
  resetToMainMenu();
  ensureClient();
  requestUpdate();
}

void BookStoreActivity::onExit() {
  client.reset();
  Activity::onExit();
}

void BookStoreActivity::resetToMainMenu() {
  state = BookStoreState::MainMenu;
  mainMenuIndex = 0;
  resultSelectedIndex = 0;
  currentPage = 1;
  books.clear();
  books.shrink_to_fit();
}

void BookStoreActivity::ensureClient() {
  if (!client) {
    client = std::make_unique<BookStoreClient>();
    client->setBaseUrl(SETTINGS.bookStoreBaseUrl);
    client->setCredentials(SETTINGS.bookStoreEmail, SETTINGS.bookStorePassword);
  }
}

void BookStoreActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    if (state == BookStoreState::MainMenu) {
      finish();
    } else {
      resetToMainMenu();
      requestUpdate();
    }
    return;
  }

  switch (state) {
    case BookStoreState::MainMenu:
      if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
        if (mainMenuIndex == 0) {
          state = BookStoreState::SearchInput;
          searchQuery.clear();
        } else {
          state = BookStoreState::Settings;
          settingsIndex = 0;
        }
        requestUpdate();
      }
      buttonNavigator.onNext([this] {
        mainMenuIndex = ButtonNavigator::nextIndex(mainMenuIndex, MAIN_MENU_ITEM_COUNT);
        requestUpdate();
      });
      buttonNavigator.onPrevious([this] {
        mainMenuIndex = ButtonNavigator::previousIndex(mainMenuIndex, MAIN_MENU_ITEM_COUNT);
        requestUpdate();
      });
      break;

    case BookStoreState::Settings:
      if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
        static constexpr StrId labels[SETTINGS_ITEM_COUNT] = {
            StrId::STR_BOOK_STORE_BASE_URL, StrId::STR_BOOK_STORE_EMAIL, StrId::STR_BOOK_STORE_PASSWORD,
            StrId::STR_BOOK_STORE_DOWNLOAD_PATH};
        char* target = nullptr;
        size_t maxLen = 0;
        InputType inputType = InputType::Text;
        switch (settingsIndex) {
          case 0:
            target = SETTINGS.bookStoreBaseUrl;
            maxLen = sizeof(SETTINGS.bookStoreBaseUrl);
            inputType = InputType::Url;
            break;
          case 1:
            target = SETTINGS.bookStoreEmail;
            maxLen = sizeof(SETTINGS.bookStoreEmail);
            inputType = InputType::Text;
            break;
          case 2:
            target = SETTINGS.bookStorePassword;
            maxLen = sizeof(SETTINGS.bookStorePassword);
            inputType = InputType::Password;
            break;
          case 3:
            target = SETTINGS.bookStoreDownloadPath;
            maxLen = sizeof(SETTINGS.bookStoreDownloadPath);
            inputType = InputType::Text;
            break;
        }
        if (target) {
          startActivityForResult(
              std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, I18N.get(labels[settingsIndex]),
                                                      std::string(target), maxLen, inputType),
              [this, target, maxLen](const ActivityResult& result) {
                if (!result.isCancelled && std::holds_alternative<KeyboardResult>(result.data)) {
                  const auto& keyboardResult = std::get<KeyboardResult>(result.data);
                  std::strncpy(target, keyboardResult.text.c_str(), maxLen - 1);
                  target[maxLen - 1] = '\0';
                  SETTINGS.saveToFile();
                }
                requestUpdate();
              });
        }
      }
      buttonNavigator.onNext([this] {
        settingsIndex = ButtonNavigator::nextIndex(settingsIndex, SETTINGS_ITEM_COUNT);
        requestUpdate();
      });
      buttonNavigator.onPrevious([this] {
        settingsIndex = ButtonNavigator::previousIndex(settingsIndex, SETTINGS_ITEM_COUNT);
        requestUpdate();
      });
      break;

    // Other states handled in later tasks
    default:
      break;
  }
}

void BookStoreActivity::render(RenderLock&&) {
  renderer.clearScreen();

  switch (state) {
    case BookStoreState::MainMenu:
      renderMainMenu();
      break;
    case BookStoreState::Settings:
      renderSettings();
      break;
    case BookStoreState::SearchInput:
      renderSearchInput();
      break;
    case BookStoreState::Searching:
      renderSearching();
      break;
    case BookStoreState::ResultsList:
      renderResultsList();
      break;
    case BookStoreState::BookDetails:
      renderBookDetails();
      break;
    case BookStoreState::ConfirmDownload:
      renderConfirmDownload();
      break;
    case BookStoreState::Downloading:
      renderDownloading();
      break;
    case BookStoreState::DownloadDone:
      renderDownloadDone();
      break;
    case BookStoreState::Error:
      renderError();
      break;
  }

  renderer.displayBuffer();
}

void BookStoreActivity::renderSettings() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_BOOK_STORE_SETTINGS));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;

  static constexpr StrId labels[SETTINGS_ITEM_COUNT] = {
      StrId::STR_BOOK_STORE_BASE_URL, StrId::STR_BOOK_STORE_EMAIL, StrId::STR_BOOK_STORE_PASSWORD,
      StrId::STR_BOOK_STORE_DOWNLOAD_PATH};
  const char* values[SETTINGS_ITEM_COUNT] = {SETTINGS.bookStoreBaseUrl, SETTINGS.bookStoreEmail,
                                             SETTINGS.bookStorePassword, SETTINGS.bookStoreDownloadPath};

  GUI.drawList(renderer, Rect{0, contentTop, pageWidth, contentHeight}, SETTINGS_ITEM_COUNT, settingsIndex,
               [](int index) { return std::string(I18N.get(labels[index])); },
               [values](int index) { return std::string(values[index]); }, nullptr);

  const auto labelsHints = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labelsHints.btn1, labelsHints.btn2, labelsHints.btn3, labelsHints.btn4);
}

void BookStoreActivity::renderMainMenu() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_BOOK_STORE));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;

  static constexpr StrId menuItems[MAIN_MENU_ITEM_COUNT] = {StrId::STR_BOOK_STORE_SEARCH,
                                                            StrId::STR_BOOK_STORE_SETTINGS};
  static constexpr StrId menuDescs[MAIN_MENU_ITEM_COUNT] = {StrId::STR_EMPTY, StrId::STR_EMPTY};
  static constexpr UIIcon menuIcons[MAIN_MENU_ITEM_COUNT] = {UIIcon::Book, UIIcon::Settings};

  GUI.drawList(renderer, Rect{0, contentTop, pageWidth, contentHeight}, MAIN_MENU_ITEM_COUNT, mainMenuIndex,
               [](int index) { return std::string(I18N.get(menuItems[index])); },
               [](int index) { return std::string(I18N.get(menuDescs[index])); },
               [](int index) { return menuIcons[index]; });

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}
