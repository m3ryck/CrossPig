#include "BookStoreActivity.h"

#include <I18n.h>
#include <Logging.h>

#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

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
            StrId::STR_BOOK_STORE_DOWNLOAD_PATH, StrId::STR_BOOK_STORE_VERIFY_LOGIN};
        if (settingsIndex == SETTINGS_ITEM_COUNT - 1) {
          // Verify credentials
          ensureClient();
          BookStoreError err = client->login();
          if (err == BookStoreError::Ok) {
            std::strncpy(verificationMessage, tr(STR_BOOK_STORE_LOGIN_OK), sizeof(verificationMessage) - 1);
          } else {
            std::strncpy(verificationMessage, client->getLastErrorMessage(), sizeof(verificationMessage) - 1);
          }
          verificationMessage[sizeof(verificationMessage) - 1] = '\0';
          state = BookStoreState::VerificationResult;
          requestUpdate();
        } else {
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

    case BookStoreState::SearchInput:
      if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
        startActivityForResult(
            std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_BOOK_STORE_SEARCH), searchQuery, 64,
                                                    InputType::Text),
            [this](const ActivityResult& result) {
              if (!result.isCancelled && std::holds_alternative<KeyboardResult>(result.data)) {
                const auto& keyboardResult = std::get<KeyboardResult>(result.data);
                searchQuery = keyboardResult.text;
                if (!searchQuery.empty()) {
                  startSearch();
                } else {
                  requestUpdate();
                }
              } else {
                resetToMainMenu();
                requestUpdate();
              }
            });
      }
      break;

    case BookStoreState::ResultsList:
      if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
        if (resultSelectedIndex >= 0 && resultSelectedIndex < static_cast<int>(books.size())) {
          selectedBook = books[resultSelectedIndex];
          state = BookStoreState::BookDetails;
          requestUpdate();
        }
      }
      buttonNavigator.onNext([this] {
        resultSelectedIndex = ButtonNavigator::nextIndex(resultSelectedIndex, static_cast<int>(books.size()));
        requestUpdate();
      });
      buttonNavigator.onPrevious([this] {
        resultSelectedIndex = ButtonNavigator::previousIndex(resultSelectedIndex, static_cast<int>(books.size()));
        requestUpdate();
      });
      if (mappedInput.wasPressed(MappedInputManager::Button::PageForward)) {
        currentPage++;
        startSearch();
      }
      if (mappedInput.wasPressed(MappedInputManager::Button::PageBack)) {
        if (currentPage > 1) {
          currentPage--;
          startSearch();
        }
      }
      break;

    case BookStoreState::BookDetails:
      if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
        state = BookStoreState::ConfirmDownload;
        requestUpdate();
      }
      break;

    case BookStoreState::ConfirmDownload:
      if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
        state = BookStoreState::Downloading;
        downloadProgress = 0;
        downloadTotal = 0;
        cancelDownload = false;
        requestUpdate();

        ensureClient();
        BookStoreError err = client->resolveDownloadUrl(selectedBook, downloadUrl);
        onDownloadLinkResolved(err);
      } else if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
        state = BookStoreState::BookDetails;
        requestUpdate();
      }
      break;

    case BookStoreState::Downloading:
      if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
        cancelDownload = true;
      }
      break;

    case BookStoreState::DownloadDone:
      if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
        activityManager.goToReader(downloadPath, false);
      }
      break;

    case BookStoreState::Error:
      if (mappedInput.wasReleased(MappedInputManager::Button::Confirm) ||
          mappedInput.wasPressed(MappedInputManager::Button::Back)) {
        resetToMainMenu();
        requestUpdate();
      }
      break;

    // Other states handled in later tasks
    default:
      break;
  }
}

void BookStoreActivity::startSearch() {
  state = BookStoreState::Searching;
  books.clear();
  resultSelectedIndex = 0;
  requestUpdate();

  ensureClient();
  BookStoreError err = client->search(searchQuery.c_str(), currentPage, books);
  onSearchCompleted(err);
}

void BookStoreActivity::onSearchCompleted(BookStoreError err) {
  if (err != BookStoreError::Ok) {
    lastError = err;
    state = BookStoreState::Error;
  } else {
    state = BookStoreState::ResultsList;
  }
  requestUpdate();
}

void BookStoreActivity::onDownloadLinkResolved(BookStoreError err) {
  if (err != BookStoreError::Ok) {
    lastError = err;
    state = BookStoreState::Error;
    requestUpdate();
    return;
  }

  buildDownloadPath();
  BookStoreError downloadErr = client->downloadFile(
      downloadUrl, downloadPath,
      [this](size_t downloaded, size_t total) {
        downloadProgress = downloaded;
        downloadTotal = total;
        requestUpdate(true);
      },
      &cancelDownload);
  onDownloadCompleted(downloadErr);
}

void BookStoreActivity::buildDownloadPath() {
  char fileName[160];
  std::snprintf(fileName, sizeof(fileName), "%s_%s.%s", selectedBook.title, selectedBook.id,
                selectedBook.extension);
  // Sanitize filename: replace path separators and spaces
  for (size_t i = 0; fileName[i] != '\0'; i++) {
    if (fileName[i] == '/' || fileName[i] == '\\' || fileName[i] == ' ') {
      fileName[i] = '_';
    }
  }

  downloadPath = std::string(SETTINGS.bookStoreDownloadPath) + "/" + fileName;
}

void BookStoreActivity::onDownloadCompleted(BookStoreError err) {
  if (err != BookStoreError::Ok) {
    lastError = err;
    state = BookStoreState::Error;
  } else {
    state = BookStoreState::DownloadDone;
  }
  requestUpdate();
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
    case BookStoreState::VerificationResult:
      renderVerificationResult();
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
      StrId::STR_BOOK_STORE_DOWNLOAD_PATH, StrId::STR_BOOK_STORE_VERIFY_LOGIN};
  const char* values[SETTINGS_ITEM_COUNT] = {SETTINGS.bookStoreBaseUrl, SETTINGS.bookStoreEmail,
                                             SETTINGS.bookStorePassword, SETTINGS.bookStoreDownloadPath, ""};

  GUI.drawList(renderer, Rect{0, contentTop, pageWidth, contentHeight}, SETTINGS_ITEM_COUNT, settingsIndex,
               [](int index) { return std::string(I18N.get(labels[index])); },
               [values](int index) { return std::string(values[index]); }, nullptr);

  const auto labelsHints = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labelsHints.btn1, labelsHints.btn2, labelsHints.btn3, labelsHints.btn4);
}

void BookStoreActivity::renderSearchInput() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_BOOK_STORE_SEARCH));

  // Centered prompt
  const int y = renderer.getScreenHeight() / 2;
  renderer.drawCenteredText(UI_10_FONT_ID, y, tr(STR_BOOK_STORE_SEARCH), true);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_EMPTY), tr(STR_EMPTY));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void BookStoreActivity::renderSearching() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_BOOK_STORE));

  const int y = renderer.getScreenHeight() / 2;
  renderer.drawCenteredText(UI_10_FONT_ID, y, tr(STR_LOADING), true);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_EMPTY), tr(STR_EMPTY), tr(STR_EMPTY));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void BookStoreActivity::renderResultsList() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  char header[64];
  std::snprintf(header, sizeof(header), "%s (%u)", I18N.get(StrId::STR_BOOK_STORE_SEARCH), currentPage);
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, header);

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;

  GUI.drawList(renderer, Rect{0, contentTop, pageWidth, contentHeight}, static_cast<int>(books.size()),
               resultSelectedIndex,
               [this](int index) {
                 return std::string(books[index].title);
               },
               [this](int index) {
                 char buf[128];
                 std::snprintf(buf, sizeof(buf), "%s / %s / %s", books[index].author, books[index].extension,
                               books[index].filesizeString);
                 return std::string(buf);
               },
               [](int) { return UIIcon::Book; });

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void BookStoreActivity::renderBookDetails() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_BOOK_STORE));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;

  char buf[256];
  std::snprintf(buf, sizeof(buf), "%s\n%s\n%s, %s\n%s", selectedBook.title, selectedBook.author,
                selectedBook.extension, selectedBook.filesizeString, selectedBook.language);

  const int lineHeight = renderer.getLineHeight(UI_10_FONT_ID);
  const int x = metrics.contentSidePadding;
  const int maxWidth = pageWidth - metrics.contentSidePadding * 2;
  const int maxLines = contentHeight / lineHeight;
  const auto lines = renderer.wrappedText(UI_10_FONT_ID, buf, maxWidth, maxLines);

  int y = contentTop;
  for (const auto& line : lines) {
    if (y + lineHeight > contentTop + contentHeight) break;
    renderer.drawText(UI_10_FONT_ID, x, y, line.c_str(), true);
    y += lineHeight;
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_BOOK_STORE_DOWNLOADING), tr(STR_EMPTY), tr(STR_EMPTY));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void BookStoreActivity::renderConfirmDownload() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_BOOK_STORE));

  const int y = renderer.getScreenHeight() / 2 - renderer.getLineHeight(UI_10_FONT_ID);
  char msg[192];
  std::snprintf(msg, sizeof(msg), "%s\n%s (%s, %s)", I18N.get(StrId::STR_BOOK_STORE_CONFIRM_DOWNLOAD),
                selectedBook.title, selectedBook.extension, selectedBook.filesizeString);

  renderer.drawCenteredText(UI_10_FONT_ID, y, msg, true);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_CONFIRM), tr(STR_EMPTY), tr(STR_EMPTY));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
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

void BookStoreActivity::renderDownloading() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_BOOK_STORE));

  const int y = renderer.getScreenHeight() / 2 - renderer.getLineHeight(UI_10_FONT_ID);
  char msg[128];
  if (downloadTotal > 0) {
    std::snprintf(msg, sizeof(msg), "%s\n%zu / %zu bytes", I18N.get(StrId::STR_BOOK_STORE_DOWNLOADING),
                  downloadProgress, downloadTotal);
  } else {
    std::snprintf(msg, sizeof(msg), "%s\n%zu bytes", I18N.get(StrId::STR_BOOK_STORE_DOWNLOADING), downloadProgress);
  }

  renderer.drawCenteredText(UI_10_FONT_ID, y, msg, true);

  const auto labels = mappedInput.mapLabels(tr(STR_CANCEL), tr(STR_EMPTY), tr(STR_EMPTY), tr(STR_EMPTY));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void BookStoreActivity::renderDownloadDone() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_BOOK_STORE));

  const int y = renderer.getScreenHeight() / 2 - renderer.getLineHeight(UI_10_FONT_ID);
  renderer.drawCenteredText(UI_10_FONT_ID, y, tr(STR_BOOK_STORE_DOWNLOAD_COMPLETE), true);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_BOOK_STORE_OPEN_BOOK), tr(STR_EMPTY), tr(STR_EMPTY));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void BookStoreActivity::renderVerificationResult() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_BOOK_STORE));

  const int y = renderer.getScreenHeight() / 2 - renderer.getLineHeight(UI_10_FONT_ID);
  renderer.drawCenteredText(UI_10_FONT_ID, y, verificationMessage, true);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_EMPTY), tr(STR_EMPTY), tr(STR_EMPTY));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void BookStoreActivity::renderError() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_BOOK_STORE));

  const int y = renderer.getScreenHeight() / 2 - renderer.getLineHeight(UI_10_FONT_ID);
  const char* msg = tr(STR_BOOK_STORE_DOWNLOAD_FAILED);
  switch (lastError) {
    case BookStoreError::Auth:
      msg = tr(STR_BOOK_STORE_LOGIN_FAILED);
      break;
    case BookStoreError::Quota:
      msg = tr(STR_BOOK_STORE_QUOTA_REACHED);
      break;
    case BookStoreError::NotFound:
      msg = tr(STR_BOOK_STORE_NO_RESULTS);
      break;
    case BookStoreError::Network:
      msg = tr(STR_BOOK_STORE_NO_WIFI);
      break;
    case BookStoreError::File:
      msg = tr(STR_BOOK_STORE_SAVE_FAILED);
      break;
    case BookStoreError::Cancelled:
      msg = tr(STR_BOOK_STORE_CANCELED);
      break;
    default:
      break;
  }

  renderer.drawCenteredText(UI_10_FONT_ID, y, msg, true);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_OK), tr(STR_EMPTY), tr(STR_EMPTY));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}
