#ifndef ANDROID_USB_AUDIO_DEVICE_H
#define ANDROID_USB_AUDIO_DEVICE_H

#include <string>

/*
 * Obtains a USB Audio Class device file descriptor on Android, for tac_usb.
 *
 * Android will not let a process enumerate USB directly: a device is opened
 * through android.hardware.usb.UsbManager on the Java side, and what crosses
 * to native is the file descriptor from UsbDeviceConnection. That is why
 * tac_usb::open() takes an fd and nothing else.
 *
 * Everything here is done by JNI reflection against the platform classes, so
 * this needs no Java source file and no change to the Android harness. The
 * JavaVM arrives through JNI_OnLoad, which the runtime calls when
 * MainActivity runs System.loadLibrary("itgmania_android") — this code is
 * compiled into that same shared object, and nothing else in it defines
 * JNI_OnLoad.
 */
namespace itgmania_usb {

/*
 * Finds the first device exposing a USB audio streaming interface, opens it,
 * and returns its file descriptor.
 *
 * Returns -1 on failure and fills [error] with something worth showing a
 * person. Requests permission if the app does not already hold it and waits
 * a bounded time for an answer, because the grant is a dialog the user has
 * to tap.
 *
 * The returned descriptor stays valid only while the UsbDeviceConnection
 * that produced it is alive, so this holds a global reference to it until
 * Release(). Do not close the descriptor directly.
 */
int AcquireDeviceFd(std::string* error);

/* Closes the connection and drops the reference. Invalidates the descriptor
 * from AcquireDeviceFd, so stop tac_usb first. Safe to call when nothing
 * was acquired. */
void Release();

}  // namespace itgmania_usb

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
