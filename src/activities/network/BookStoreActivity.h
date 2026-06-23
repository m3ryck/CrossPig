#pragma once

#include "network/BookStoreClient.h"

#include <memory>
#include <string>
#include <vector>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

enum class BookStoreState {
  MainMenu,
  Settings,
  SearchInput,
  Searching,
  ResultsList,
  BookDetails,
  ConfirmDownload,
  Downloading,
  DownloadDone,
  VerificationResult,
  Error
};

class BookStoreActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  BookStoreState state = BookStoreState::MainMenu;

  // Main menu
  int mainMenuIndex = 0;
  static constexpr int MAIN_MENU_ITEM_COUNT = 2;

  // Settings
  int settingsIndex = 0;
  static constexpr int SETTINGS_ITEM_COUNT = 5;

  // Search input
  std::string searchQuery;

  // Results
  std::vector<BookStoreBook> books;
  int resultSelectedIndex = 0;
  uint32_t currentPage = 1;

  // Download
  BookStoreBook selectedBook;
  std::string downloadUrl;
  std::string downloadPath;
  size_t downloadProgress = 0;
  size_t downloadTotal = 0;
  bool cancelDownload = false;

  // Async client
  std::unique_ptr<BookStoreClient> client;

  // Verification result
  char verificationMessage[128] = {0};

  // Error
  BookStoreError lastError = BookStoreError::Ok;

 public:
  explicit BookStoreActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("BookStoreActivity", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  void resetToMainMenu();
  void startSearch();
  void onSearchCompleted(BookStoreError err);
  void onDownloadLinkResolved(BookStoreError err);
  void onDownloadCompleted(BookStoreError err);
  void ensureClient();
  void buildDownloadPath();

  // Render helpers
  void renderMainMenu();
  void renderSettings();
  void renderSearchInput();
  void renderSearching();
  void renderResultsList();
  void renderBookDetails();
  void renderConfirmDownload();
  void renderDownloading();
  void renderDownloadDone();
  void renderVerificationResult();
  void renderError();
};
