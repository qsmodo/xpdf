//========================================================================
//
// XPDFApp.h
//
// Copyright 2002-2003 Glyph & Cog, LLC
//
//========================================================================

#ifndef XPDFAPP_H
#define XPDFAPP_H

#include <poppler-config.h>

#include <memory>
#include <string>
#include <vector>
#define Object XtObject
#include <Xm/XmAll.h>
#undef Object
#include <splash/SplashTypes.h>
#include "config.h"

class PDFDoc;
class XPDFViewer;

//------------------------------------------------------------------------

#define xpdfAppName "Xpdf"

//------------------------------------------------------------------------
// XPDFApp
//------------------------------------------------------------------------

class XPDFApp {
public:

  XPDFApp(int *argc, char *argv[]);
  ~XPDFApp();

  XPDFViewer *open(const std::string *fileName, int page = 1,
		   const std::string *dest = NULL,
		   const std::string *ownerPassword = NULL,
		   const std::string *userPassword = NULL);
  XPDFViewer *reopen(XPDFViewer *viewer, PDFDoc *doc, int page,
		     bool fullScreenA);
  void close(XPDFViewer *viewer, bool closeLast);
  void quit();

  void run();

  //----- remote server
  void setRemoteName(char *remoteName);
  bool remoteServerRunning();
  void remoteExec(const std::string& cmd);
  void remoteOpen(const std::string& fileName, int page, bool raise);
  void remoteOpenAtDest(const std::string& fileName, const std::string& dest,
			bool raise);
  void remoteReload(bool raise);
  void remoteRaise();
  void remoteQuit();

  //----- resource/option values
  const std::string& getGeometry() { return geometry; }
  const std::string& getTitle() { return title; }
  bool getInstallCmap() { return installCmap; }
  int getRGBCubeSize() { return rgbCubeSize; }
  bool getReverseVideo() { return reverseVideo; }
  SplashColorPtr getPaperRGB() { return paperRGB; }
  unsigned long getPaperPixel() { return paperPixel; }
  unsigned long getMattePixel(bool fullScreenA)
    { return fullScreenA ? fullScreenMattePixel : mattePixel; }
  const std::string& getInitialZoom() { return initialZoom; }
  void setFullScreen(bool fullScreenA) { fullScreen = fullScreenA; }
  bool getFullScreen() { return fullScreen; }

  XtAppContext getAppContext() { return appContext; }
  Widget getAppShell() { return appShell; }

private:

  void getResources();
  void remoteSend(const std::string& cmd);
  static void remoteMsgCbk(Widget widget, XtPointer ptr,
			   XEvent *event, Boolean *cont);

  Display *display;
  int screenNum;
  XtAppContext appContext;
  Widget appShell;
  std::vector<std::unique_ptr<XPDFViewer>> viewers;

  Atom remoteAtom;
  Window remoteXWin;
  XPDFViewer *remoteViewer;
  Widget remoteWin;

  //----- resource/option values
  std::string geometry;
  std::string title;
  bool installCmap;
  int rgbCubeSize;
  bool reverseVideo;
  SplashColor paperRGB;
  unsigned long paperPixel;
  unsigned long mattePixel;
  unsigned long fullScreenMattePixel;
  std::string initialZoom;
  bool fullScreen;
};

#endif
