//========================================================================
//
// CoreOutputDev.h
//
// Copyright 2004 Glyph & Cog, LLC
//
//========================================================================

#ifndef COREOUTPUTDEV_H
#define COREOUTPUTDEV_H

#include <splash/SplashTypes.h>
#include <SplashOutputDev.h>

class TextPage;

//------------------------------------------------------------------------

typedef void (*CoreOutRedrawCbk)(void *data, int x0, int y0, int x1, int y1,
				 bool composited);

//------------------------------------------------------------------------
// CoreOutputDev
//------------------------------------------------------------------------

class CoreOutputDev: public SplashOutputDev {
public:

  CoreOutputDev(SplashColorMode colorModeA, int bitmapRowPadA,
		bool reverseVideoA, SplashColorPtr paperColorA,
		CoreOutRedrawCbk redrawCbkA,
		void *redrawCbkDataA);

  virtual ~CoreOutputDev();

  //----- initialization and control

  // End a page.
  virtual void endPage();

  //----- special access

  // Clear out the document (used when displaying an empty window).
  void clear();

private:

  CoreOutRedrawCbk redrawCbk;
  void *redrawCbkData;
};

#endif
