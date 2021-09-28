//========================================================================
//
// CoreOutputDev.cc
//
// Copyright 2004 Glyph & Cog, LLC
//
//========================================================================

#include <poppler-config.h>

#include "Object.h"
#include "TextOutputDev.h"
#include "CoreOutputDev.h"

//------------------------------------------------------------------------
// CoreOutputDev
//------------------------------------------------------------------------

CoreOutputDev::CoreOutputDev(SplashColorMode colorModeA, int bitmapRowPadA,
			     bool reverseVideoA, SplashColorPtr paperColorA,
			     CoreOutRedrawCbk redrawCbkA,
			     void *redrawCbkDataA):
  SplashOutputDev(colorModeA, bitmapRowPadA, reverseVideoA, paperColorA)
{
  redrawCbk = redrawCbkA;
  redrawCbkData = redrawCbkDataA;
}

CoreOutputDev::~CoreOutputDev() {
}

void CoreOutputDev::endPage() {
  SplashOutputDev::endPage();
  (*redrawCbk)(redrawCbkData, 0, 0, getBitmapWidth(), getBitmapHeight(),
               true);
}

void CoreOutputDev::clear() {
  startDoc(NULL);
#ifdef STARTPAGE_XREF
  startPage(0, NULL, NULL);
#else
  startPage(0, NULL);
#endif
}
