#ifndef LOADING_WINDOW_H
#define LOADING_WINDOW_H

#include <string>

struct RageSurface;
/** @brief Opens and displays the loading banner. */
class LoadingWindow {
 public:
  static LoadingWindow* Create();

  /* Defaulted inline, with the members initialized in-class below. It used to
   * be defined in LoadingWindow.cpp, which meant any build that supplies its
   * own loading window and drops that file -- the Android port does exactly
   * that -- failed to link on this constructor alone. */
  LoadingWindow() = default;
  virtual std::string Init() { return std::string(); }
  virtual ~LoadingWindow() {}

  virtual void SetText(std::string str) = 0;
  virtual void SetIcon(const RageSurface* pIcon) {}
  virtual void SetSplash(const RageSurface* pSplash) {}
  virtual void SetProgress(const int progress) { m_progress = progress; }
  virtual void SetTotalWork(const int totalWork) { m_totalWork = totalWork; }
  virtual void SetIndeterminate(bool indeterminate) {
    m_indeterminate = indeterminate;
  }

 protected:
  int m_progress = 0;
  int m_totalWork = 1;
  bool m_indeterminate = false;
};

#endif

/**
 * @file
 * @author Glenn Maynard (c) 2002-2004
 * @section LICENSE
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
