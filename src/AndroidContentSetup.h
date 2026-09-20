#ifndef ANDROID_CONTENT_SETUP_H
#define ANDROID_CONTENT_SETUP_H

#include <string>

/*
 * Prepares user-supplied content on Android and reports where it lives.
 *
 * Ties together the two halves that are useless apart: AndroidStorage, which
 * gets permission to read shared storage at all, and SongTreeImport, which
 * turns whatever a person dropped there into the two-level tree SongManager
 * scans.
 *
 * Content lives at <shared>/ITGmania/:
 *
 *   Songs/   mounted at /Songs and read in place -- a correctly laid out pack
 *            dropped straight in here just works, with nothing copied.
 *   Import/  anywhere else. Whatever lands here gets normalized into Songs/,
 *            which is what rescues a pack unzipped one level short or deep.
 *
 * Call once during startup, after FILEMAN and PrefsManager exist and before
 * SongManager scans. Importing after the scan would leave the songs invisible
 * until the next launch.
 */
namespace AndroidContentSetup {

/* Ensures the content directories exist, imports anything waiting in Import/,
 * and returns the Songs directory for the caller to mount.
 *
 * Returns empty when shared storage cannot be read -- which on a first run is
 * the normal case, since the All-files access grant is a screen the person has
 * to visit. This opens that screen and returns empty; the next launch finds
 * the grant in place. Startup continues either way, on whatever content the
 * app already has.
 */
std::string Prepare();

}  // namespace AndroidContentSetup

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
