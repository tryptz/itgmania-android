#include "RageSoundDriver_LibusbUAC.h"

#if !defined(_WIN32)
#include <sys/resource.h>
#endif

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>

#include "PrefsManager.h"
#include "RageLog.h"
#include "RageSound.h"
#include "RageUtil.h"
#include "global.h"
#include "libusb_uac_driver.h"

REGISTER_SOUND_DRIVER_CLASS2("LibusbUAC", LibusbUAC);

using monotrypt::usb::LibusbUacDriver;
using monotrypt::usb::StartError;

static const int channels = 2;

/* Cap on how long the mixing thread parks in waitWritable(). Bounded so
 * that shutdown never waits on the device: the thread re-checks
 * m_bShutdown each time round. */
static const int kWaitTimeoutMs = 20;

/* Frames of writeahead we try to keep buffered. Lower is tighter sync,
 * higher survives scheduler jitter. ITGmania exposes SoundWriteAhead as a
 * preference; honour it when set, as the ALSA driver does. */
static const int kDefaultWriteahead = 2048;

/* Android passes this in from UsbDeviceConnection.getFileDescriptor(). */
static int g_iDeviceFd = -1;

void RageSoundDriver_LibusbUAC::SetDeviceFd(int fd) { g_iDeviceFd = fd; }

RageSoundDriver_LibusbUAC::RageSoundDriver_LibusbUAC()
    : m_pDriver(nullptr),
      m_bShutdown(false),
      m_iSampleRate(0),
      m_iBytesPerSample(0),
      m_iChannels(channels),
      m_iRingFrames(0) {}

std::string RageSoundDriver_LibusbUAC::OpenDevice() {
  if (g_iDeviceFd < 0) {
    /* A desktop build would open by VID/PID here and detach the kernel's
     * snd-usb-audio driver from the streaming interface. tac_usb has no
     * such entry point today — open() takes a file descriptor only,
     * because on Android the OS will not let a process enumerate USB. Until
     * tac_usb grows openByVidPid(), this driver is Android-only. */
    return "No USB device descriptor was supplied (SetDeviceFd was never "
           "called). This driver currently requires an Android host.";
  }

  if (!m_pDriver->ensureContext()) {
    return "Could not create a libusb context.";
  }
  if (!m_pDriver->open(g_iDeviceFd)) {
    return ssprintf("Could not open USB device on fd %i.", g_iDeviceFd);
  }
  return "";
}

std::string RageSoundDriver_LibusbUAC::Init() {
  m_pDriver = new LibusbUacDriver();

  std::string sError = OpenDevice();
  if (sError != "") {
    return sError;
  }

  /* Ask for the DAC's best common ground with what we can produce. We mix in
   * float, so a 24-bit subslot is genuinely more resolution than the int16
   * path would carry; try widest first and fall back. */
  int iRate = PREFSMAN->m_iSoundPreferredSampleRate;
  if (iRate <= 0) {
    iRate = 44100;
  }

  static const int kBitDepths[] = {24, 32, 16};
  bool bStarted = false;
  for (int iBits : kBitDepths) {
    if (m_pDriver->start(iRate, iBits, channels)) {
      bStarted = true;
      break;
    }
    LOG->Trace(
        "LibusbUAC: %i-bit at %i Hz refused (%i); trying next depth", iBits,
        iRate, static_cast<int>(m_pDriver->lastError()));
  }
  if (!bStarted) {
    const StartError err = m_pDriver->lastError();
    std::string sDetail = m_pDriver->lastErrorDetail();
    if (err == StartError::ClaimInterfaceFailed) {
      return "The kernel's USB audio driver still owns this DAC. Enable "
             "Developer Options -> \"Disable USB audio routing\" and "
             "reconnect it.";
    }
    return ssprintf(
        "Could not start the USB audio stream (error %i)%s%s",
        static_cast<int>(err), sDetail.empty() ? "" : ": ", sDetail.c_str());
  }

  const monotrypt::usb::StreamFormat& fmt = m_pDriver->currentFormat();
  m_iSampleRate = fmt.sampleRateHz;
  m_iBytesPerSample = fmt.bytesPerSample;
  m_iChannels = fmt.channels;

  m_iRingFrames = m_pDriver->ringFrames();

  LOG->Info(
      "LibusbUAC: %i Hz, %i-bit (%i-byte subslot), %i ch, ring %i frames "
      "(%.0f ms), pump queue %i frames (%.1f ms)",
      m_iSampleRate, fmt.bitsPerSample, m_iBytesPerSample, m_iChannels,
      m_iRingFrames, 1000.0f * m_iRingFrames / m_iSampleRate,
      m_pDriver->inflightFrames(),
      1000.0f * m_pDriver->inflightFrames() / m_iSampleRate);

  StartDecodeThread();

  m_MixingThread.SetName("RageSoundDriver_LibusbUAC");
  m_MixingThread.Create(MixerThread_start, this);

  return "";
}

RageSoundDriver_LibusbUAC::~RageSoundDriver_LibusbUAC() {
  if (m_MixingThread.IsCreated()) {
    m_bShutdown = true;
    LOG->Trace("Shutting down mixer thread ...");
    m_MixingThread.Wait();
    LOG->Trace("Mixer thread shut down.");
  }

  if (m_pDriver != nullptr) {
    m_pDriver->stop();
    m_pDriver->close();
    delete m_pDriver;
    m_pDriver = nullptr;
  }
}

int RageSoundDriver_LibusbUAC::MixerThread_start(void* p) {
  ((RageSoundDriver_LibusbUAC*)p)->MixerThread();
  return 0;
}

void RageSoundDriver_LibusbUAC::MixerThread() {
  while (!m_bShutdown) {
    while (!m_bShutdown && GetData());

    /* Park until the pump frees a block or the timeout expires. The
     * timeout is what lets m_bShutdown be noticed promptly; waitWritable
     * also returns early if the stream goes down underneath us. */
    m_pDriver->waitWritable(samples_per_block, kWaitTimeoutMs);
  }
}

void RageSoundDriver_LibusbUAC::ConvertForDevice(const float* pIn,
                                                 int iFrames) {
  const int iSamples = iFrames * m_iChannels;
  m_ConvBuf.resize(static_cast<size_t>(iSamples) * m_iBytesPerSample);
  uint8_t* pOut = m_ConvBuf.data();

  for (int i = 0; i < iSamples; ++i) {
    /* Clamp before scaling: Mix() sums voices and can exceed [-1,1], and
     * wrapping a hot mix into a DAC is the one failure mode that damages
     * equipment rather than just sounding wrong. */
    float f = pIn[i];
    f = std::max(-1.0f, std::min(1.0f, f));

    switch (m_iBytesPerSample) {
      case 2: {
        const int32_t v = static_cast<int32_t>(f * 32767.0f);
        pOut[0] = static_cast<uint8_t>(v & 0xFF);
        pOut[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
        pOut += 2;
        break;
      }
      case 3: {
        const int32_t v = static_cast<int32_t>(f * 8388607.0f);
        pOut[0] = static_cast<uint8_t>(v & 0xFF);
        pOut[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
        pOut[2] = static_cast<uint8_t>((v >> 16) & 0xFF);
        pOut += 3;
        break;
      }
      default: {
        const int32_t v = static_cast<int32_t>(f * 2147483520.0f);
        pOut[0] = static_cast<uint8_t>(v & 0xFF);
        pOut[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
        pOut[2] = static_cast<uint8_t>((v >> 16) & 0xFF);
        pOut[3] = static_cast<uint8_t>((v >> 24) & 0xFF);
        pOut += 4;
        break;
      }
    }
  }
}

bool RageSoundDriver_LibusbUAC::GetData() {
  int iWriteahead = kDefaultWriteahead;
  if (PREFSMAN->m_iSoundWriteAhead) {
    iWriteahead = PREFSMAN->m_iSoundWriteAhead;
  }

  /* Only fill up to the writeahead, not to the top of tac_usb's ~250 ms
   * ring. Filling the whole ring would put a quarter second between Mix()
   * and audibility, which is a quarter second of stale audio to flush on
   * every seek. */
  const int iWritable = m_pDriver->writableFrames();
  const int iBuffered = m_iRingFrames - iWritable;
  const int iFramesToFill = std::min(iWritable, iWriteahead - iBuffered);
  if (iFramesToFill <= 0) {
    return false;
  }

  m_MixBuf.resize(static_cast<size_t>(iFramesToFill) * m_iChannels);

  /* iFrameNumber is when this block will be heard; iCurrentFrame is what is
   * audible now. Both must be in the same clock as GetPosition(), or start
   * timing lands in the wrong place. */
  const int64_t iCurrentFrame = GetPosition();
  const int64_t iFrameNumber = iCurrentFrame + iBuffered;

  this->Mix(m_MixBuf.data(), iFramesToFill, iFrameNumber, iCurrentFrame);

  ConvertForDevice(m_MixBuf.data(), iFramesToFill);

  const int iWritten = m_pDriver->writePcm(m_ConvBuf.data(), iFramesToFill);
  if (iWritten < iFramesToFill) {
    /* writePcm() is allowed to short-write when the ring filled underneath
     * us. We sized the request from writableFrames() on this same thread and
     * we are the only producer, so this means the accounting disagrees. */
    LOG->Warn(
        "LibusbUAC: short write, %i of %i frames", iWritten, iFramesToFill);
  }

  return true;
}

int64_t RageSoundDriver_LibusbUAC::GetPosition() const {
  /* audibleFrames(), never playedFrames(). The raw pump counter includes
   * underrun padding and leads the DAC by the queue depth; tac_usb
   * subtracts both. That distinction is not cosmetic here:
   * ClampHardwareFrame() latches the running maximum to keep position
   * monotonic, so drift from counting padding would never be given back
   * and one underrun would offset the chart for the rest of the song.
   *
   * The DAC's own rate-matching FIFO is still unaccounted for. The device
   * does not report it, it is constant, and it lands in the player's
   * global offset calibration along with display and input latency. */
  return m_pDriver->audibleFrames();
}

float RageSoundDriver_LibusbUAC::GetPlayLatency() const {
  if (m_iSampleRate <= 0) {
    return 0.0f;
  }
  int iWriteahead = kDefaultWriteahead;
  if (PREFSMAN->m_iSoundWriteAhead) {
    iWriteahead = PREFSMAN->m_iSoundWriteAhead;
  }
  return float(iWriteahead + m_pDriver->inflightFrames()) / m_iSampleRate;
}

void RageSoundDriver_LibusbUAC::SetupDecodingThread() {
#if !defined(_WIN32)
  setpriority(PRIO_PROCESS, 0, -5);
#endif
}

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
