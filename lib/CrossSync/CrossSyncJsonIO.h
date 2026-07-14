#pragma once

class CrossSyncCredentialStore;

namespace CrossSyncJsonIO {
bool save(const CrossSyncCredentialStore& store, const char* path);
bool load(CrossSyncCredentialStore& store, const char* json, bool* needsResave);
}  // namespace CrossSyncJsonIO
