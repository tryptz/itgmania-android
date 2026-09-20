#ifndef ANDROID_STORAGE_H
#define ANDROID_STORAGE_H

#include <string>

/*
 * Reaching user-visible storage on Android.
 *
 * The port writes everything under getExternalFilesDir(), which needs no
 * permission -- but from Android 11 that directory is not browsable by file
 * managers and is hidden over MTP on most devices, so a person has no
 * practical way to put song folders there. Songs are directories of .sm/.ssc
 * charts plus audio and images, and chart files are not media, so
 * READ_MEDIA_AUDIO cannot see them either.
 *
 * That leaves All-files access (MANAGE_EXTERNAL_STORAGE), which is what
 * sideloaded ports use. It keeps raw POSIX paths working, so ITGMania's
 * RageFileDriverDirect and the song importer need no changes at all. The
 * alternative, the Storage Access Framework, hands out opaque document URIs
 * and would mean writing a whole RageFileDriver over DocumentFile.
 *
 * The permission must also be declared in AndroidManifest.xml:
 *
 *   <uses-permission android:name="android.permission.MANAGE_EXTERNAL_STORAGE" />
 *
 * That part cannot be done from native code. Without it the settings screen
 * below will not list the app and access can never be granted.
 */
namespace AndroidStorage {

/* True when the app may read arbitrary paths on shared storage. Always true
 * below API 30, where the legacy storage permissions apply instead. */
bool HasAllFilesAccess();

/* Opens the system screen where a person grants All-files access, and returns
 * once it has been shown -- the grant itself happens outside the app, so poll
 * HasAllFilesAccess() afterwards rather than treating this as the answer.
 * Returns false if the screen could not be opened. */
bool RequestAllFilesAccess(std::string* error);

/* Shared storage root, normally /storage/emulated/0. Empty on failure. */
std::string GetPublicStorageRoot(std::string* error);

/* Where a person is expected to put content: <shared>/ITGmania/. Empty if the
 * shared root could not be resolved. Creating it is the caller's business. */
std::string GetPublicContentDir(std::string* error);

}  // namespace AndroidStorage

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
