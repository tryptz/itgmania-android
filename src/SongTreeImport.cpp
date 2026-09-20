#include "SongTreeImport.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

namespace {

/* Chart formats SongManager will actually load. Compared case-folded, because
 * packs from the wild ship .SM and .SSC as often as the lowercase spelling and
 * a case-sensitive match would silently import nothing on a case-sensitive
 * filesystem -- which is every Android device. */
const char* const kChartExtensions[] = {".sm", ".ssc", ".dwi"};

std::string ToLower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return s;
}

bool IsChartFile(const fs::path& p) {
  const std::string ext = ToLower(p.extension().string());
  for (const char* known : kChartExtensions) {
    if (ext == known) {
      return true;
    }
  }
  return false;
}

/* Makes a name safe to use as a single path component. Song and group names
 * are taken from whatever the user's folders happen to be called, so they can
 * carry separators, leading dots that would hide the folder, or trailing dots
 * and spaces. */
std::string SanitizeComponent(
    const std::string& raw, const std::string& fallback) {
  std::string out;
  out.reserve(raw.size());
  for (const char c : raw) {
    const unsigned char u = static_cast<unsigned char>(c);
    if (c == '/' || c == '\\' || u < 0x20) {
      out += '_';
    } else {
      out += c;
    }
  }
  // Trailing dots and spaces are legal here but confuse enough tools that
  // stripping them is worth more than preserving them exactly.
  while (!out.empty() && (out.back() == ' ' || out.back() == '.')) {
    out.pop_back();
  }
  while (!out.empty() && out.front() == ' ') {
    out.erase(out.begin());
  }
  if (out.empty() || out == "." || out == "..") {
    return fallback;
  }
  return out;
}

/* Copies [from] to [to], directories and all. Returns false and fills [error]
 * on failure. */
bool CopyTree(const fs::path& from, const fs::path& to, std::string* error) {
  std::error_code ec;
  fs::create_directories(to, ec);
  if (ec) {
    *error = "could not create " + to.string() + ": " + ec.message();
    return false;
  }
  fs::copy(
      from, to,
      fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
  if (ec) {
    *error = "could not copy " + from.string() + ": " + ec.message();
    return false;
  }
  return true;
}

/* Copies only the files sitting directly in [from] -- no subdirectories. Used
 * when charts are loose in the import root, where recursing would drag in
 * every unrelated thing the user keeps there. */
bool CopyFilesOnly(const fs::path& from, const fs::path& to, std::string* error) {
  std::error_code ec;
  fs::create_directories(to, ec);
  if (ec) {
    *error = "could not create " + to.string() + ": " + ec.message();
    return false;
  }
  for (fs::directory_iterator it(from, ec), end; !ec && it != end;
       it.increment(ec)) {
    if (!it->is_regular_file(ec)) {
      continue;
    }
    std::error_code copy_ec;
    fs::copy_file(
        it->path(), to / it->path().filename(),
        fs::copy_options::overwrite_existing, copy_ec);
    if (copy_ec) {
      *error = "could not copy " + it->path().string() + ": " + copy_ec.message();
      return false;
    }
  }
  return true;
}

}  // namespace

namespace SongTreeImport {

bool DirectoryHoldsChart(const std::string& dir) {
  std::error_code ec;
  for (fs::directory_iterator it(dir, ec), end; !ec && it != end;
       it.increment(ec)) {
    if (it->is_regular_file(ec) && IsChartFile(it->path())) {
      return true;
    }
  }
  return false;
}

Result Import(
    const std::string& importRoot, const std::string& songsRoot,
    const std::string& defaultGroup) {
  Result result;

  std::error_code ec;
  const fs::path root = fs::path(importRoot);
  if (!fs::is_directory(root, ec)) {
    result.warnings.push_back("Import folder does not exist: " + importRoot);
    return result;
  }

  const fs::path songs = fs::path(songsRoot);
  fs::create_directories(songs, ec);
  if (ec) {
    result.warnings.push_back(
        "Could not create songs folder " + songsRoot + ": " + ec.message());
    return result;
  }

  const std::string safeDefaultGroup =
      SanitizeComponent(defaultGroup, "Imported");

  // Charts sitting loose in the import root: treat the root as one song rather
  // than recursing, so unrelated files kept alongside them are not swept in.
  if (DirectoryHoldsChart(importRoot)) {
    ++result.chartsFound;
    std::string songName;
    for (fs::directory_iterator it(root, ec), end; !ec && it != end;
         it.increment(ec)) {
      if (it->is_regular_file(ec) && IsChartFile(it->path())) {
        songName = SanitizeComponent(it->path().stem().string(), "Untitled");
        break;
      }
    }
    const fs::path dest = songs / safeDefaultGroup / songName;
    if (fs::exists(dest, ec)) {
      ++result.songsSkipped;
    } else {
      std::string error;
      if (CopyFilesOnly(root, dest, &error)) {
        ++result.songsImported;
      } else {
        ++result.songsFailed;
        result.warnings.push_back(error);
      }
    }
  }

  // Everything else: walk down and take each directory that holds a chart.
  // Once one is found we do not descend into it, so a song's own subfolders
  // are never mistaken for songs of their own.
  fs::recursive_directory_iterator it(
      root, fs::directory_options::skip_permission_denied, ec);
  if (ec) {
    result.warnings.push_back(
        "Could not read import folder " + importRoot + ": " + ec.message());
    return result;
  }

  for (fs::recursive_directory_iterator end; it != end; it.increment(ec)) {
    if (ec) {
      result.warnings.push_back("Skipped an unreadable entry: " + ec.message());
      ec.clear();
      continue;
    }
    if (!it->is_directory(ec)) {
      continue;
    }
    const fs::path dir = it->path();
    if (!DirectoryHoldsChart(dir.string())) {
      continue;
    }

    ++result.chartsFound;
    it.disable_recursion_pending();  // this is a song, not a group

    const std::string songName =
        SanitizeComponent(dir.filename().string(), "Untitled");

    // The parent folder is the pack, and therefore the group -- unless the
    // song sits directly in the import root, in which case there is no pack
    // and everything lands in the default group.
    std::string groupName = safeDefaultGroup;
    const fs::path parent = dir.parent_path();
    if (parent != root && !parent.filename().empty()) {
      groupName = SanitizeComponent(parent.filename().string(), safeDefaultGroup);
    }
    // A group named the same as its only song reads as a mistake and produces
    // Songs/X/X/. Harmless, but the default group is clearer.
    if (groupName == songName) {
      groupName = safeDefaultGroup;
    }

    const fs::path dest = songs / groupName / songName;
    if (fs::exists(dest, ec)) {
      ++result.songsSkipped;
      continue;
    }

    std::string error;
    if (CopyTree(dir, dest, &error)) {
      ++result.songsImported;
    } else {
      ++result.songsFailed;
      result.warnings.push_back(error);
    }
  }

  return result;
}

}  // namespace SongTreeImport

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
