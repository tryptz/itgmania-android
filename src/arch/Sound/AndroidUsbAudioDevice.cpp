#include "AndroidUsbAudioDevice.h"

#include <jni.h>

#include <chrono>
#include <string>
#include <thread>

#include "AndroidJni.h"

namespace {

using itgmania_jni::CheckAndClearException;
using itgmania_jni::GetApplicationContext;
using itgmania_jni::Local;
using itgmania_jni::ScopedEnv;

/* The open connection. Its file descriptor dies with it, so the reference is
 * held for as long as tac_usb is streaming. */
jobject g_connection = nullptr;

/* Constants from android.hardware.usb.UsbConstants and android.app.PendingIntent.
 * Hardcoded rather than read back through JNI: they are frozen platform ABI,
 * and reading them would be six more reflection calls that can fail. */
constexpr jint kUsbClassAudio = 1;
constexpr jint kUsbSubclassAudioStreaming = 2;
constexpr jint kPendingIntentFlagImmutable = 0x04000000;  // API 23+, required from 31

constexpr const char* kPermissionAction = "org.itgmania.android.USB_PERMISSION";

/* How long to wait for the user to answer the permission dialog. Bounded
 * because this runs inside the sound driver's Init(): too short and nobody
 * can reach the dialog, too long and a declined prompt hangs startup. */
constexpr int kPermissionWaitMs = 15000;
constexpr int kPermissionPollMs = 100;

/* True if any interface on the device is USB audio streaming. Checking the
 * interfaces rather than the device class is deliberate: a UAC device reports
 * class 0 (per-interface) at the device level, so filtering on device class
 * would match nothing. */
bool IsAudioDevice(JNIEnv* env, jclass deviceClass, jobject device) {
  const jmethodID getInterfaceCount =
      env->GetMethodID(deviceClass, "getInterfaceCount", "()I");
  const jmethodID getInterface = env->GetMethodID(
      deviceClass, "getInterface", "(I)Landroid/hardware/usb/UsbInterface;");
  if (getInterfaceCount == nullptr || getInterface == nullptr) {
    env->ExceptionClear();
    return false;
  }

  const jint count = env->CallIntMethod(device, getInterfaceCount);
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    return false;
  }

  for (jint i = 0; i < count; ++i) {
    Local<jobject> iface(env, env->CallObjectMethod(device, getInterface, i));
    if (!iface || env->ExceptionCheck()) {
      env->ExceptionClear();
      continue;
    }
    Local<jclass> ifaceClass(env, env->GetObjectClass(iface.get()));
    const jmethodID getClass =
        env->GetMethodID(ifaceClass.get(), "getInterfaceClass", "()I");
    const jmethodID getSubclass =
        env->GetMethodID(ifaceClass.get(), "getInterfaceSubclass", "()I");
    if (getClass == nullptr || getSubclass == nullptr) {
      env->ExceptionClear();
      continue;
    }
    const jint cls = env->CallIntMethod(iface.get(), getClass);
    const jint sub = env->CallIntMethod(iface.get(), getSubclass);
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      continue;
    }
    if (cls == kUsbClassAudio && sub == kUsbSubclassAudioStreaming) {
      return true;
    }
  }
  return false;
}

/* Puts up the system permission dialog and waits for an answer.
 *
 * The grant result normally arrives as a broadcast, which would need a
 * BroadcastReceiver and therefore a Java class. hasPermission() flips as soon
 * as the user accepts, so polling it reaches the same answer without one —
 * at the cost of up to one poll interval of latency, which nobody can
 * perceive against a dialog a human is tapping.
 */
bool RequestPermission(JNIEnv* env, jobject usbManager, jclass managerClass,
                       jobject context, jobject device, jmethodID hasPermission,
                       std::string* error) {
  Local<jclass> intentClass(env, env->FindClass("android/content/Intent"));
  Local<jclass> pendingClass(env, env->FindClass("android/app/PendingIntent"));
  if (!intentClass || !pendingClass) {
    env->ExceptionClear();
    *error = "Could not reach Intent/PendingIntent";
    return false;
  }

  const jmethodID intentCtor =
      env->GetMethodID(intentClass.get(), "<init>", "(Ljava/lang/String;)V");
  const jmethodID getBroadcast = env->GetStaticMethodID(
      pendingClass.get(), "getBroadcast",
      "(Landroid/content/Context;ILandroid/content/Intent;I)"
      "Landroid/app/PendingIntent;");
  const jmethodID requestPermission =
      env->GetMethodID(managerClass, "requestPermission",
                       "(Landroid/hardware/usb/UsbDevice;"
                       "Landroid/app/PendingIntent;)V");
  if (intentCtor == nullptr || getBroadcast == nullptr ||
      requestPermission == nullptr) {
    env->ExceptionClear();
    *error = "Could not reach the USB permission API";
    return false;
  }

  Local<jstring> action(env, env->NewStringUTF(kPermissionAction));
  Local<jobject> intent(
      env, env->NewObject(intentClass.get(), intentCtor, action.get()));
  if (!intent || CheckAndClearException(env, "new Intent", error)) {
    return false;
  }

  /* FLAG_IMMUTABLE is mandatory from API 31; harmless before it. */
  Local<jobject> pending(
      env, env->CallStaticObjectMethod(pendingClass.get(), getBroadcast, context,
                                       0, intent.get(),
                                       kPendingIntentFlagImmutable));
  if (!pending || CheckAndClearException(env, "PendingIntent.getBroadcast", error)) {
    return false;
  }

  env->CallVoidMethod(usbManager, requestPermission, device, pending.get());
  if (CheckAndClearException(env, "UsbManager.requestPermission", error)) {
    return false;
  }

  for (int waited = 0; waited < kPermissionWaitMs; waited += kPermissionPollMs) {
    std::this_thread::sleep_for(std::chrono::milliseconds(kPermissionPollMs));
    const jboolean granted =
        env->CallBooleanMethod(usbManager, hasPermission, device);
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      continue;
    }
    if (granted == JNI_TRUE) {
      return true;
    }
  }

  *error = "Permission to use the USB audio device was not granted";
  return false;
}

}  // namespace

namespace itgmania_usb {

int AcquireDeviceFd(std::string* error) {
  std::string sink;
  if (error == nullptr) {
    error = &sink;
  }
  error->clear();

  if (itgmania_jni::GetJavaVM() == nullptr) {
    *error = "No JavaVM — JNI_OnLoad never ran, so this is not an Android build";
    return -1;
  }

  ScopedEnv scoped;
  if (!scoped) {
    *error = "Could not attach this thread to the JVM";
    return -1;
  }
  JNIEnv* env = scoped.get();

  Local<jobject> context(env, GetApplicationContext(env, error));
  if (!context) {
    if (error->empty()) *error = "No Application context";
    return -1;
  }

  Local<jclass> contextClass(env, env->GetObjectClass(context.get()));
  const jmethodID getSystemService =
      env->GetMethodID(contextClass.get(), "getSystemService",
                       "(Ljava/lang/String;)Ljava/lang/Object;");
  if (getSystemService == nullptr) {
    env->ExceptionClear();
    *error = "Context.getSystemService not found";
    return -1;
  }

  Local<jstring> usbService(env, env->NewStringUTF("usb"));
  Local<jobject> usbManager(env, env->CallObjectMethod(
                                     context.get(), getSystemService,
                                     usbService.get()));
  if (!usbManager || CheckAndClearException(env, "getSystemService(usb)", error)) {
    if (error->empty()) *error = "No USB service on this device";
    return -1;
  }

  Local<jclass> managerClass(env, env->GetObjectClass(usbManager.get()));
  const jmethodID getDeviceList = env->GetMethodID(
      managerClass.get(), "getDeviceList", "()Ljava/util/HashMap;");
  const jmethodID hasPermission =
      env->GetMethodID(managerClass.get(), "hasPermission",
                       "(Landroid/hardware/usb/UsbDevice;)Z");
  const jmethodID openDevice =
      env->GetMethodID(managerClass.get(), "openDevice",
                       "(Landroid/hardware/usb/UsbDevice;)"
                       "Landroid/hardware/usb/UsbDeviceConnection;");
  if (getDeviceList == nullptr || hasPermission == nullptr ||
      openDevice == nullptr) {
    env->ExceptionClear();
    *error = "UsbManager is missing expected methods";
    return -1;
  }

  Local<jobject> deviceMap(
      env, env->CallObjectMethod(usbManager.get(), getDeviceList));
  if (!deviceMap || CheckAndClearException(env, "getDeviceList", error)) {
    if (error->empty()) *error = "Could not list USB devices";
    return -1;
  }

  Local<jclass> mapClass(env, env->GetObjectClass(deviceMap.get()));
  const jmethodID values =
      env->GetMethodID(mapClass.get(), "values", "()Ljava/util/Collection;");
  Local<jobject> collection(
      env, values != nullptr ? env->CallObjectMethod(deviceMap.get(), values)
                             : nullptr);
  if (!collection || CheckAndClearException(env, "HashMap.values", error)) {
    if (error->empty()) *error = "Could not read the USB device list";
    return -1;
  }

  Local<jclass> collectionClass(env, env->GetObjectClass(collection.get()));
  const jmethodID iterator = env->GetMethodID(collectionClass.get(), "iterator",
                                              "()Ljava/util/Iterator;");
  Local<jobject> it(env, iterator != nullptr ? env->CallObjectMethod(
                                                   collection.get(), iterator)
                                             : nullptr);
  if (!it || CheckAndClearException(env, "Collection.iterator", error)) {
    if (error->empty()) *error = "Could not iterate USB devices";
    return -1;
  }

  Local<jclass> iteratorClass(env, env->GetObjectClass(it.get()));
  const jmethodID hasNext = env->GetMethodID(iteratorClass.get(), "hasNext", "()Z");
  const jmethodID next =
      env->GetMethodID(iteratorClass.get(), "next", "()Ljava/lang/Object;");
  if (hasNext == nullptr || next == nullptr) {
    env->ExceptionClear();
    *error = "Iterator is missing expected methods";
    return -1;
  }

  int devicesSeen = 0;
  while (env->CallBooleanMethod(it.get(), hasNext) == JNI_TRUE) {
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      break;
    }
    Local<jobject> device(env, env->CallObjectMethod(it.get(), next));
    if (!device || env->ExceptionCheck()) {
      env->ExceptionClear();
      continue;
    }
    ++devicesSeen;

    Local<jclass> deviceClass(env, env->GetObjectClass(device.get()));
    if (!IsAudioDevice(env, deviceClass.get(), device.get())) {
      continue;
    }

    jboolean granted =
        env->CallBooleanMethod(usbManager.get(), hasPermission, device.get());
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      granted = JNI_FALSE;
    }
    if (granted != JNI_TRUE) {
      if (!RequestPermission(env, usbManager.get(), managerClass.get(),
                             context.get(), device.get(), hasPermission,
                             error)) {
        return -1;  // error already set, and it is the actionable one
      }
    }

    jobject connection =
        env->CallObjectMethod(usbManager.get(), openDevice, device.get());
    if (connection == nullptr || CheckAndClearException(env, "UsbManager.openDevice", error)) {
      if (error->empty()) {
        *error =
            "openDevice() returned null — another process may hold the DAC";
      }
      return -1;
    }

    Local<jclass> connectionClass(env, env->GetObjectClass(connection));
    const jmethodID getFd =
        env->GetMethodID(connectionClass.get(), "getFileDescriptor", "()I");
    if (getFd == nullptr) {
      env->ExceptionClear();
      env->DeleteLocalRef(connection);
      *error = "UsbDeviceConnection.getFileDescriptor not found";
      return -1;
    }
    const jint fd = env->CallIntMethod(connection, getFd);
    if (fd < 0 || CheckAndClearException(env, "getFileDescriptor", error)) {
      env->DeleteLocalRef(connection);
      if (error->empty()) *error = "The USB connection gave no descriptor";
      return -1;
    }

    /* Hold the connection: the descriptor is owned by it, and letting it be
     * collected would pull the fd out from under the iso pump. */
    Release();
    g_connection = env->NewGlobalRef(connection);
    env->DeleteLocalRef(connection);
    return static_cast<int>(fd);
  }

  if (devicesSeen == 0) {
    *error = "No USB devices are attached";
  } else {
    *error = "No USB audio device found among the attached devices";
  }
  return -1;
}

void Release() {
  if (g_connection == nullptr || itgmania_jni::GetJavaVM() == nullptr) {
    return;
  }
  ScopedEnv scoped;
  if (!scoped) {
    return;
  }
  JNIEnv* env = scoped.get();
  Local<jclass> connectionClass(env, env->GetObjectClass(g_connection));
  const jmethodID close = env->GetMethodID(connectionClass.get(), "close", "()V");
  if (close != nullptr) {
    env->CallVoidMethod(g_connection, close);
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
    }
  }
  env->DeleteGlobalRef(g_connection);
  g_connection = nullptr;
}

}  // namespace itgmania_usb

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
