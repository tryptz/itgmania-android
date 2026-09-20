#include "AndroidJni.h"

#include <jni.h>

#include <string>

namespace {

JavaVM* g_vm = nullptr;

/* Android declares AttachCurrentThread taking JNIEnv**, the desktop JDK takes
 * void**. Only difference that matters for building this on a host to check
 * it. */
#if defined(__ANDROID__)
using AttachArg = JNIEnv**;
#else
using AttachArg = void**;
#endif

}  // namespace

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* /*reserved*/) {
  g_vm = vm;
  return JNI_VERSION_1_6;
}

namespace itgmania_jni {

JavaVM* GetJavaVM() { return g_vm; }

ScopedEnv::ScopedEnv() {
  if (g_vm == nullptr) {
    return;
  }
  void* raw = nullptr;
  const jint rc = g_vm->GetEnv(&raw, JNI_VERSION_1_6);
  if (rc == JNI_OK) {
    env_ = static_cast<JNIEnv*>(raw);
  } else if (rc == JNI_EDETACHED) {
    if (g_vm->AttachCurrentThread(reinterpret_cast<AttachArg>(&env_),
                                  nullptr) == JNI_OK) {
      attached_ = true;
    } else {
      env_ = nullptr;
    }
  }
}

ScopedEnv::~ScopedEnv() {
  if (attached_ && g_vm != nullptr) {
    g_vm->DetachCurrentThread();
  }
}

bool CheckAndClearException(
    JNIEnv* env, const char* what, std::string* error) {
  if (env->ExceptionCheck()) {
    env->ExceptionDescribe();  // goes to logcat, where it is actually readable
    env->ExceptionClear();
    if (error != nullptr) {
      *error = std::string(what) + " threw a Java exception (see logcat)";
    }
    return true;
  }
  return false;
}

jobject GetApplicationContext(JNIEnv* env, std::string* error) {
  Local<jclass> activityThread(env, env->FindClass("android/app/ActivityThread"));
  if (!activityThread ||
      CheckAndClearException(env, "FindClass(ActivityThread)", error)) {
    if (error != nullptr && error->empty()) {
      *error = "android.app.ActivityThread not found";
    }
    return nullptr;
  }
  const jmethodID currentApplication = env->GetStaticMethodID(
      activityThread.get(), "currentApplication", "()Landroid/app/Application;");
  if (currentApplication == nullptr ||
      CheckAndClearException(env, "GetStaticMethodID(currentApplication)",
                             error)) {
    if (error != nullptr && error->empty()) {
      *error = "ActivityThread.currentApplication() not found";
    }
    return nullptr;
  }
  jobject context =
      env->CallStaticObjectMethod(activityThread.get(), currentApplication);
  if (CheckAndClearException(env, "currentApplication()", error)) {
    return nullptr;
  }
  if (context == nullptr && error != nullptr) {
    *error = "No Application context yet -- the process is not fully started";
  }
  return context;
}

}  // namespace itgmania_jni

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
