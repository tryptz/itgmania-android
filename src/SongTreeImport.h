#ifndef SONG_TREE_IMPORT_H
#define SONG_TREE_IMPORT_H

#include <string>
#include <vector>

/*
 * Normalizes arbitrary .sm/.ssc content into the song tree SongManager scans.
 *
 * SongManager::LoadSongDir walks exactly two levels — Songs/<Group>/<Song>/ —
 * and nothing else is seen. Three shapes come out of the wild and none of them
 * load as-is:
 *
 *   Songs/<Song>/song.sm              one level short. <Song> is taken for a
 *                                     group, SanityCheckGroupDir finds audio
 *                                     sitting in it, and the WHOLE GROUP is
 *                                     dropped with only a log warning — so one
 *                                     badly-unzipped pack silently costs every
 *                                     song in it.
 *   Songs/<Pack>/<Sub>/<Song>/song.sm one level too deep. Never enumerated.
 *   Songs/song.sm                     a loose chart. Never enumerated.
 *
 * This finds every directory that actually holds a chart, wherever it sits,
 * and materializes it at exactly Songs/<Group>/<Song>/.
 */
namespace SongTreeImport {

struct Result {
  int chartsFound = 0;     // directories holding at least one chart
  int songsImported = 0;   // newly placed in the tree
  int songsSkipped = 0;    // destination already existed
  int songsFailed = 0;     // copy failed; see warnings
  std::vector<std::string> warnings;

  bool AnyWork() const { return songsImported > 0; }
};

/*
 * Imports every song found under [importRoot] into [songsRoot].
 *
 * The group name comes from the chart directory's parent when there is one
 * (a pack folder), otherwise [defaultGroup]. A song whose destination already
 * exists is left alone rather than overwritten, so re-running after dropping
 * in new files is cheap and safe.
 *
 * [importRoot] is left untouched: files are copied, not moved, because the
 * import folder is usually the user's own download directory and eating its
 * contents would be a surprise. Neither path needs to be inside the mounted
 * filesystem — this works on raw OS paths, which is what an Android
 * /sdcard location is.
 */
Result Import(
    const std::string& importRoot, const std::string& songsRoot,
    const std::string& defaultGroup = "Imported");

/* True if [dir] directly contains a file this game would load as a chart.
 * Exposed for callers that want to probe a single directory. */
bool DirectoryHoldsChart(const std::string& dir);

}  // namespace SongTreeImport

#endif

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
