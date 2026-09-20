#include "AndroidStorage.h"

#include <jni.h>

#include <string>

#include "AndroidJni.h"

namespace {

using itgmania_jni::CheckAndClearException;
using itgmania_jni::GetApplicationContext;
using itgmania_jni::Local;
using itgmania_jni::ScopedEnv;

/* android.provider.Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION.
 * Hardcoded rather than read back through JNI: it is frozen platform ABI, and
 * reading it would be three more reflection calls that can fail. */
constexpr const char* kManageAllFilesAction =
    "android.settings.MANAGE_APP_ALL_FILES_ACCESS_PERMISSION";

/* Intent.FLAG_ACTIVITY_NEW_TASK. Required: we start the settings screen from
 * an Application context, not an Activity, and Android refuses that without
 * this flag. */
constexpr jint kFlagActivityNewTask = 0x10000000;

std::string JStringToStd(JNIEnv* env, jstring value) {
  if (value == nullptr) {
    return {};
  }
  const char* chars = env->GetStringUTFChars(value, nullptr);
  std::string out = chars != nullptr ? chars : "";
  if (chars != nullptr) {
    env->ReleaseStringUTFChars(value, chars);
  }
  return out;
}

void SetIfEmpty(std::string* error, const char* text) {
  if (error != nullptr && error->empty()) {
    *error = text;
  }
}

}  // namespace

namespace AndroidStorage {

bool HasAllFilesAccess() {
  ScopedEnv scoped;
  if (!scoped) {
    return false;
  }
  JNIEnv* env = scoped.get();

  Local<jclass> environment(env, env->FindClass("android/os/Environment"));
  if (!environment) {
    env->ExceptionClear();
    return false;
  }
  const jmethodID isManager = env->GetStaticMethodID(
      environment.get(), "isExternalStorageManager", "()Z");
  if (isManager == nullptr) {
    // Below API 30 the method does not exist. There, the legacy storage
    // permissions govern access instead, so report the capability as present
    // and let the actual file operation be the judge.
    env->ExceptionClear();
    return true;
  }
  const jboolean granted =
      env->CallStaticBooleanMethod(environment.get(), isManager);
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    return false;
  }
  return granted == JNI_TRUE;
}

bool RequestAllFilesAccess(std::string* error) {
  std::string sink;
  if (error == nullptr) {
    error = &sink;
  }
  error->clear();

  ScopedEnv scoped;
  if (!scoped) {
    *error = "Could not attach this thread to the JVM";
    return false;
  }
  JNIEnv* env = scoped.get();

  Local<jobject> context(env, GetApplicationContext(env, error));
  if (!context) {
    SetIfEmpty(error, "No Application context");
    return false;
  }

  Local<jclass> contextClass(env, env->GetObjectClass(context.get()));
  const jmethodID getPackageName = env->GetMethodID(
      contextClass.get(), "getPackageName", "()Ljava/lang/String;");
  const jmethodID startActivity = env->GetMethodID(
      contextClass.get(), "startActivity", "(Landroid/content/Intent;)V");
  if (getPackageName == nullptr || startActivity == nullptr) {
    env->ExceptionClear();
    *error = "Context is missing expected methods";
    return false;
  }

  Local<jstring> packageName(
      env, static_cast<jstring>(
               env->CallObjectMethod(context.get(), getPackageName)));
  if (!packageName || CheckAndClearException(env, "getPackageName", error)) {
    SetIfEmpty(error, "Could not read the package name");
    return false;
  }

  // The settings screen needs "package:<name>" as its data, or it opens the
  // global list instead of this app's entry.
  const std::string uriText =
      "package:" + JStringToStd(env, packageName.get());

  Local<jclass> uriClass(env, env->FindClass("android/net/Uri"));
  Local<jclass> intentClass(env, env->FindClass("android/content/Intent"));
  if (!uriClass || !intentClass) {
    env->ExceptionClear();
    *error = "Could not reach Uri/Intent";
    return false;
  }

  const jmethodID uriParse = env->GetStaticMethodID(
      uriClass.get(), "parse", "(Ljava/lang/String;)Landroid/net/Uri;");
  const jmethodID intentCtor = env->GetMethodID(
      intentClass.get(), "<init>",
      "(Ljava/lang/String;Landroid/net/Uri;)V");
  const jmethodID addFlags = env->GetMethodID(
      intentClass.get(), "addFlags", "(I)Landroid/content/Intent;");
  if (uriParse == nullptr || intentCtor == nullptr || addFlags == nullptr) {
    env->ExceptionClear();
    *error = "Could not reach the Intent API";
    return false;
  }

  Local<jstring> uriString(env, env->NewStringUTF(uriText.c_str()));
  Local<jobject> uri(
      env, env->CallStaticObjectMethod(uriClass.get(), uriParse,
                                       uriString.get()));
  if (!uri || CheckAndClearException(env, "Uri.parse", error)) {
    SetIfEmpty(error, "Could not build the settings URI");
    return false;
  }

  Local<jstring> action(env, env->NewStringUTF(kManageAllFilesAction));
  Local<jobject> intent(
      env, env->NewObject(intentClass.get(), intentCtor, action.get(),
                          uri.get()));
  if (!intent || CheckAndClearException(env, "new Intent", error)) {
    SetIfEmpty(error, "Could not build the settings intent");
    return false;
  }

  Local<jobject> flagged(
      env, env->CallObjectMethod(intent.get(), addFlags, kFlagActivityNewTask));
  if (CheckAndClearException(env, "Intent.addFlags", error)) {
    return false;
  }

  env->CallVoidMethod(context.get(), startActivity, intent.get());
  if (CheckAndClearException(env, "startActivity", error)) {
    SetIfEmpty(error,
               "This device has no All-files access screen. Check that "
               "MANAGE_EXTERNAL_STORAGE is declared in the manifest.");
    return false;
  }
  return true;
}

std::string GetPublicStorageRoot(std::string* error) {
  std::string sink;
  if (error == nullptr) {
    error = &sink;
  }
  error->clear();

  ScopedEnv scoped;
  if (!scoped) {
    *error = "Could not attach this thread to the JVM";
    return {};
  }
  JNIEnv* env = scoped.get();

  Local<jclass> environment(env, env->FindClass("android/os/Environment"));
  if (!environment) {
    env->ExceptionClear();
    *error = "android.os.Environment not found";
    return {};
  }
  const jmethodID getDir = env->GetStaticMethodID(
      environment.get(), "getExternalStorageDirectory", "()Ljava/io/File;");
  if (getDir == nullptr) {
    env->ExceptionClear();
    *error = "Environment.getExternalStorageDirectory() not found";
    return {};
  }
  Local<jobject> file(
      env, env->CallStaticObjectMethod(environment.get(), getDir));
  if (!file || CheckAndClearException(env, "getExternalStorageDirectory",
                                      error)) {
    SetIfEmpty(error, "No shared storage on this device");
    return {};
  }

  Local<jclass> fileClass(env, env->GetObjectClass(file.get()));
  const jmethodID getPath = env->GetMethodID(
      fileClass.get(), "getAbsolutePath", "()Ljava/lang/String;");
  if (getPath == nullptr) {
    env->ExceptionClear();
    *error = "File.getAbsolutePath() not found";
    return {};
  }
  Local<jstring> path(
      env,
      static_cast<jstring>(env->CallObjectMethod(file.get(), getPath)));
  if (!path || CheckAndClearException(env, "getAbsolutePath", error)) {
    SetIfEmpty(error, "Could not read the shared storage path");
    return {};
  }
  return JStringToStd(env, path.get());
}

std::string GetPublicContentDir(std::string* error) {
  const std::string root = GetPublicStorageRoot(error);
  if (root.empty()) {
    return {};
  }
  if (root.back() == '/') {
    return root + "ITGmania/";
  }
  return root + "/ITGmania/";
}

}  // namespace AndroidStorage

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
