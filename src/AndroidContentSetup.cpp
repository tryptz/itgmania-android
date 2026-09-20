#include "AndroidContentSetup.h"

#include <filesystem>
#include <string>
#include <system_error>

#include "AndroidStorage.h"
#include "RageLog.h"
#include "SongTreeImport.h"

namespace fs = std::filesystem;

namespace {

/* Creates [dir] if it is not already there. Returns false and fills [error]
 * otherwise. */
bool EnsureDir(const std::string& dir, std::string* error) {
  std::error_code ec;
  fs::create_directories(dir, ec);
  if (ec && !fs::is_directory(dir, ec)) {
    *error = "could not create " + dir + ": " + ec.message();
    return false;
  }
  return true;
}

}  // namespace

namespace AndroidContentSetup {

std::string Prepare() {
  std::string error;

  if (!AndroidStorage::HasAllFilesAccess()) {
    // Expected on a first run: the grant is a settings screen someone has to
    // visit, so there is nothing to wait for here. Open it and carry on with
    // whatever content the app already has; the next launch finds the grant.
    if (AndroidStorage::RequestAllFilesAccess(&error)) {
      LOG->Info(
          "Android: opened the All-files access screen. Grant it and restart "
          "to pick up songs from shared storage.");
    } else {
      LOG->Warn(
          "Android: could not open the All-files access screen: %s. Check that "
          "MANAGE_EXTERNAL_STORAGE is declared in AndroidManifest.xml.",
          error.c_str());
    }
    return {};
  }

  const std::string content = AndroidStorage::GetPublicContentDir(&error);
  if (content.empty()) {
    LOG->Warn(
        "Android: could not resolve shared storage: %s",
        error.empty() ? "unknown reason" : error.c_str());
    return {};
  }

  const std::string songsDir = content + "Songs";
  const std::string importDir = content + "Import";
  if (!EnsureDir(songsDir, &error) || !EnsureDir(importDir, &error)) {
    LOG->Warn("Android: %s", error.c_str());
    return {};
  }

  const SongTreeImport::Result result =
      SongTreeImport::Import(importDir, songsDir);

  // Only worth a line when it did something. A quiet start is the common case
  // once everything has already been imported.
  if (result.chartsFound > 0 || !result.warnings.empty()) {
    LOG->Info(
        "Android import: %d chart folder(s) found, %d imported, %d already "
        "present, %d failed.",
        result.chartsFound, result.songsImported, result.songsSkipped,
        result.songsFailed);
  }
  for (const std::string& warning : result.warnings) {
    LOG->Warn("Android import: %s", warning.c_str());
  }

  LOG->Info("Android: mounting songs from %s", songsDir.c_str());
  return songsDir;
}

}  // namespace AndroidContentSetup

/*
 * (c) 2026 tryptz
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, and/or sell copies of the Software, and to permit persons to
 * whom the Software is furnished to do so, provided that the above
 * copyright notice(s) and this permission notice appear in all copies of
 * the Software and that both the above copyright notice(s) and this
 * permission notice appear in supporting documentation.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF
 * THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS
 * INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
