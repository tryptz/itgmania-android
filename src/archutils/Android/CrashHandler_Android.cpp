#include <android/log.h>
#include <android/set_abort_message.h>  // log.h only mentions it in comments

#include <cstdint>
#include <cstdlib>
#include <string>

#include "archutils/Unix/CrashHandler.h"

/*
 * CrashHandler for Android.
 *
 * global.cpp and RageThreads.cpp call into CrashHandler unconditionally, so
 * the symbols have to exist on every target. The Unix implementation cannot
 * supply them here: archutils/Unix/CrashHandler.cpp is listed behind CMake's
 * LINUX/APPLE, which are false for Android, and forcing it in would drag along
 * the child-process crash reporter, the backtrace machinery and the X11-era
 * pieces that go with them.
 *
 * Android does not need any of that. The platform already captures native
 * crashes -- a tombstone with a full backtrace, plus the abort message when
 * one is set -- so the useful thing to do is record the reason where the
 * tombstone will pick it up and then abort. Anything more would be a second,
 * worse copy of what the OS does.
 */
namespace {

constexpr const char* kLogTag = "ITGManiaCrash";

[[noreturn]] void AbortWithReason(const std::string& reason) {
  __android_log_print(ANDROID_LOG_FATAL, kLogTag, "%s", reason.c_str());

#if __ANDROID_API__ >= 21
  // Puts the reason in the tombstone header rather than leaving it only in
  // logcat, which is what makes a crash report readable after the fact.
  android_set_abort_message(reason.c_str());
#endif

  std::abort();
}

}  // namespace

namespace CrashHandler {

void ForceCrash(const char* reason) {
  AbortWithReason(reason != nullptr ? reason : "ForceCrash (no reason given)");
}

void ForceDeadlock(std::string reason, uint64_t CrashHandle) {
  // The handle identifies the thread the Unix handler would have backtraced.
  // We cannot backtrace another thread here, but naming it still tells you
  // which one the deadlock was blamed on.
  AbortWithReason(
      "Deadlock: " + reason + " (thread handle " +
      std::to_string(CrashHandle) + ")");
}

}  // namespace CrashHandler

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
