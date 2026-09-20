#ifndef ANDROID_JNI_H
#define ANDROID_JNI_H

#include <jni.h>

#include <string>

/*
 * Shared JNI plumbing for the Android port's native platform code.
 *
 * This exists because JNI_OnLoad is per shared object and there can only be
 * one. Several pieces of native code need a JavaVM -- the USB Audio Class
 * driver to reach UsbManager, the storage layer to reach Environment -- so the
 * one JNI_OnLoad lives here and hands the VM to whoever asks.
 *
 * The Android harness defines no JNI_OnLoad of its own, so this is it. The
 * runtime calls it when MainActivity runs System.loadLibrary, and every
 * translation unit below is compiled into that same object.
 */
namespace itgmania_jni {

/* The VM the runtime handed us, or null if JNI_OnLoad never ran (which means
 * this is not an Android process). */
JavaVM* GetJavaVM();

/* Attaches the calling thread if it is not already attached, and detaches
 * again on scope exit. Native subsystems initialise on threads the JVM has
 * never seen, so this is not optional. */
class ScopedEnv {
 public:
  ScopedEnv();
  ~ScopedEnv();
  ScopedEnv(const ScopedEnv&) = delete;
  ScopedEnv& operator=(const ScopedEnv&) = delete;

  JNIEnv* get() const { return env_; }
  explicit operator bool() const { return env_ != nullptr; }

 private:
  JNIEnv* env_ = nullptr;
  bool attached_ = false;
};

/* Deletes a local reference on scope exit. JNI guarantees only a small local
 * frame, and walking platform collections exhausts it quickly. */
template <typename T>
class Local {
 public:
  Local(JNIEnv* env, T ref) : env_(env), ref_(ref) {}
  ~Local() {
    if (ref_ != nullptr) {
      env_->DeleteLocalRef(ref_);
    }
  }
  Local(const Local&) = delete;
  Local& operator=(const Local&) = delete;

  T get() const { return ref_; }
  explicit operator bool() const { return ref_ != nullptr; }

 private:
  JNIEnv* env_;
  T ref_;
};

/* Clears a pending exception and reports it. Leaving one pending makes the
 * next JNI call abort the process, so this has to follow anything that can
 * throw. Returns true if there was an exception. */
bool CheckAndClearException(JNIEnv* env, const char* what, std::string* error);

/* The Application object, via android.app.ActivityThread.currentApplication().
 * The harness keeps its own Activity reference in an anonymous namespace where
 * it cannot be borrowed, and this is the documented way to reach a Context
 * with nothing but a JNIEnv. Returns a local reference, or null with [error]
 * filled in. */
jobject GetApplicationContext(JNIEnv* env, std::string* error);

}  // namespace itgmania_jni

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
