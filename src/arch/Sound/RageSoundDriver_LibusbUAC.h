#ifndef RAGE_SOUND_DRIVER_LIBUSB_UAC_H
#define RAGE_SOUND_DRIVER_LIBUSB_UAC_H

#include <cstdint>
#include <string>
#include <vector>

#include "RageSoundDriver.h"
#include "RageThreads.h"

namespace monotrypt {
namespace usb {
class LibusbUacDriver;
}
}  // namespace monotrypt

/*
 * Bit-perfect output straight to a USB Audio Class DAC, bypassing the
 * platform mixer entirely. Wraps tac_usb (tryptify-audio-core), which owns
 * the libusb context, the isochronous pump and an SPSC ring.
 *
 * Structurally this mirrors RageSoundDriver_ALSA9_Software: a mixing thread
 * asks the device how much room it has, calls Mix() for exactly that many
 * frames, and hands the result to the device. Three differences matter:
 *
 *  - We mix in float, not int16. RageSoundDriver offers both; the int16
 *    overload would throw away the resolution this driver exists to
 *    deliver, since the DAC commonly negotiates a 24- or 32-bit subslot.
 *
 *  - The mixing thread paces off the device via tac_usb::waitWritable()
 *    rather than polling, with a bounded timeout so shutdown is never
 *    waiting on the DAC.
 *
 *  - GetPosition() uses tac_usb::audibleFrames(), never playedFrames().
 *    The raw pump counter includes underrun padding and leads the DAC by
 *    the pump's queue depth; handing that to a rhythm game would drift the
 *    chart against the audio on every glitch. See GetPosition().
 */
class RageSoundDriver_LibusbUAC : public RageSoundDriver {
 public:
  RageSoundDriver_LibusbUAC();
  ~RageSoundDriver_LibusbUAC();

  std::string Init();

  /* virtuals: */
  int64_t GetPosition() const;
  float GetPlayLatency() const;
  int GetSampleRate() const { return m_iSampleRate; }

  void SetupDecodingThread();

  /* Android hands us a file descriptor from UsbDeviceConnection, because the
   * OS will not let a process enumerate USB directly. Call this before the
   * driver is created; Init() fails cleanly if it was never called. On a
   * desktop build this stays -1 and OpenDevice() takes the VID/PID path. */
  static void SetDeviceFd(int fd);

 private:
  static int MixerThread_start(void* p);
  void MixerThread();
  bool GetData();

  /* Acquires the USB device and hands it to tac_usb. The only
   * platform-dependent part of this driver. Returns "" on success, or a
   * human-readable error for Init() to propagate. */
  std::string OpenDevice();

  /* Converts one Mix() block (interleaved float, nominally [-1,1]) into the
   * subslot size the DAC negotiated, writing into m_ConvBuf. */
  void ConvertForDevice(const float* pIn, int iFrames);

  monotrypt::usb::LibusbUacDriver* m_pDriver;

  bool m_bShutdown;
  int m_iSampleRate;
  int m_iBytesPerSample;  // subslot size the DAC negotiated: 2, 3 or 4
  int m_iChannels;

  /* Ring capacity in frames, from tac_usb::ringFrames(). */
  int m_iRingFrames;

  std::vector<float> m_MixBuf;
  std::vector<uint8_t> m_ConvBuf;

  RageThread m_MixingThread;
};

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
