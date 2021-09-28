//========================================================================
//
// XPDFApp.cc
//
// Copyright 2002-2003 Glyph & Cog, LLC
// Copyright 2020 Adam Sampson <ats@offog.org>
//
//========================================================================

#include <poppler-config.h>

#include <goo/GooString.h>
#include "Error.h"
#include "XPDFViewer.h"
#include "XPDFApp.h"
#include "config.h"

// these macro defns conflict with xpdf's Object class
#ifdef LESSTIF_VERSION
#undef XtDisplay
#undef XtScreen
#undef XtWindow
#undef XtParent
#undef XtIsRealized
#endif

//------------------------------------------------------------------------

#define remoteCmdSize 512

//------------------------------------------------------------------------

static String fallbackResources[] = {
  "*.zoomComboBox*FontList: -*-helvetica-medium-r-normal--12-*-*-*-*-*-iso8859-1",
  "*XmTextField.FontList: -*-courier-medium-r-normal--12-*-*-*-*-*-iso8859-1",
  "*.FontList: -*-helvetica-medium-r-normal--12-*-*-*-*-*-iso8859-1",
  "*XmTextField.translations: #override\\n"
  "  Ctrl<Key>a:beginning-of-line()\\n"
  "  Ctrl<Key>b:backward-character()\\n"
  "  Ctrl<Key>d:delete-next-character()\\n"
  "  Ctrl<Key>e:end-of-line()\\n"
  "  Ctrl<Key>f:forward-character()\\n"
  "  Ctrl<Key>u:beginning-of-line()delete-to-end-of-line()\\n"
  "  Ctrl<Key>k:delete-to-end-of-line()\\n",
  "*.toolTipEnable: True",
  "*.toolTipPostDelay: 1500",
  "*.toolTipPostDuration: 0",
  "*.TipLabel.foreground: black",
  "*.TipLabel.background: LightYellow",
  "*.TipShell.borderWidth: 1",
  "*.TipShell.borderColor: black",
  NULL
};

static XrmOptionDescRec xOpts[] = {
  {"-display",       ".display",         XrmoptionSepArg,  NULL},
  {"-foreground",    "*Foreground",      XrmoptionSepArg,  NULL},
  {"-fg",            "*Foreground",      XrmoptionSepArg,  NULL},
  {"-background",    "*Background",      XrmoptionSepArg,  NULL},
  {"-bg",            "*Background",      XrmoptionSepArg,  NULL},
  {"-geometry",      ".geometry",        XrmoptionSepArg,  NULL},
  {"-g",             ".geometry",        XrmoptionSepArg,  NULL},
  {"-font",          "*.fontList",       XrmoptionSepArg,  NULL},
  {"-fn",            "*.fontList",       XrmoptionSepArg,  NULL},
  {"-title",         ".title",           XrmoptionSepArg,  NULL},
  {"-cmap",          ".installCmap",     XrmoptionNoArg,   (XPointer)"on"},
  {"-rgb",           ".rgbCubeSize",     XrmoptionSepArg,  NULL},
  {"-rv",            ".reverseVideo",    XrmoptionNoArg,   (XPointer)"true"},
  {"-papercolor",    ".paperColor",      XrmoptionSepArg,  NULL},
  {"-mattecolor",    ".matteColor",      XrmoptionSepArg,  NULL},
  {"-z",             ".initialZoom",     XrmoptionSepArg,  NULL}
};

#define nXOpts (sizeof(xOpts) / sizeof(XrmOptionDescRec))

struct XPDFAppResources {
  String geometry;
  String title;
  Bool installCmap;
  int rgbCubeSize;
  Bool reverseVideo;
  String paperColor;
  String matteColor;
  String fullScreenMatteColor;
  String initialZoom;
};

static Bool defInstallCmap = False;
static int defRGBCubeSize = defaultRGBCube;
static Bool defReverseVideo = False;

static XtResource xResources[] = {
  { "geometry",             "Geometry",             XtRString, sizeof(String), XtOffsetOf(XPDFAppResources, geometry),             XtRString, (XtPointer)NULL             },
  { "title",                "Title",                XtRString, sizeof(String), XtOffsetOf(XPDFAppResources, title),                XtRString, (XtPointer)NULL             },
  { "installCmap",          "InstallCmap",          XtRBool,   sizeof(Bool),   XtOffsetOf(XPDFAppResources, installCmap),          XtRBool,   (XtPointer)&defInstallCmap  },
  { "rgbCubeSize",          "RgbCubeSize",          XtRInt,    sizeof(int),    XtOffsetOf(XPDFAppResources, rgbCubeSize),          XtRInt,    (XtPointer)&defRGBCubeSize  },
  { "reverseVideo",         "ReverseVideo",         XtRBool,   sizeof(Bool),   XtOffsetOf(XPDFAppResources, reverseVideo),         XtRBool,   (XtPointer)&defReverseVideo },
  { "paperColor",           "PaperColor",           XtRString, sizeof(String), XtOffsetOf(XPDFAppResources, paperColor),           XtRString, (XtPointer)NULL             },
  { "matteColor",           "MatteColor",           XtRString, sizeof(String), XtOffsetOf(XPDFAppResources, matteColor),           XtRString, (XtPointer)"gray50"         },
  { "fullScreenMatteColor", "FullScreenMatteColor", XtRString, sizeof(String), XtOffsetOf(XPDFAppResources, fullScreenMatteColor), XtRString, (XtPointer)"black"          },
  { "initialZoom",          "InitialZoom",          XtRString, sizeof(String), XtOffsetOf(XPDFAppResources, initialZoom),          XtRString, (XtPointer)NULL             }
};

#define nXResources (sizeof(xResources) / sizeof(XtResource))

//------------------------------------------------------------------------
// XPDFApp
//------------------------------------------------------------------------

#if 0 //~ for debugging
static int xErrorHandler(Display *display, XErrorEvent *ev) {
  printf("X error:\n");
  printf("  resource ID = %08lx\n", ev->resourceid);
  printf("  serial = %lu\n", ev->serial);
  printf("  error_code = %d\n", ev->error_code);
  printf("  request_code = %d\n", ev->request_code);
  printf("  minor_code = %d\n", ev->minor_code);
  fflush(stdout);
  abort();
}
#endif

XPDFApp::XPDFApp(int *argc, char *argv[]) {
  appShell = XtAppInitialize(&appContext, xpdfAppName, xOpts, nXOpts,
			     argc, argv, fallbackResources, NULL, 0);
  display = XtDisplay(appShell);
  screenNum = XScreenNumberOfScreen(XtScreen(appShell));
#if XmVERSION > 1
  XtVaSetValues(XmGetXmDisplay(XtDisplay(appShell)),
		XmNenableButtonTab, True, NULL);
#endif
#if XmVERSION > 1
  // Drag-and-drop appears to be buggy -- I'm seeing weird crashes
  // deep in the Motif code when I destroy widgets in the XpdfForms
  // code.  Xpdf doesn't use it, so just turn it off.
  XtVaSetValues(XmGetXmDisplay(XtDisplay(appShell)),
		XmNdragInitiatorProtocolStyle, XmDRAG_NONE,
		XmNdragReceiverProtocolStyle, XmDRAG_NONE,
		NULL);
#endif

#if 0 //~ for debugging
  XSynchronize(display, True);
  XSetErrorHandler(&xErrorHandler);
#endif

  fullScreen = false;
  remoteAtom = None;
  remoteViewer = NULL;
  remoteWin = None;

  getResources();
}

void XPDFApp::getResources() {
  XPDFAppResources resources;
  XColor xcol, xcol2;
  Colormap colormap;
  
  XtGetApplicationResources(appShell, &resources, xResources, nXResources,
			    NULL, 0);
  geometry = resources.geometry ? resources.geometry : "";
  title = resources.title ? resources.title : "";
  installCmap = (bool)resources.installCmap;
  rgbCubeSize = resources.rgbCubeSize;
  reverseVideo = (bool)resources.reverseVideo;
  if (reverseVideo) {
    paperRGB[0] = paperRGB[1] = paperRGB[2] = 0;
    paperPixel = BlackPixel(display, screenNum);
  } else {
    paperRGB[0] = paperRGB[1] = paperRGB[2] = 0xff;
    paperPixel = WhitePixel(display, screenNum);
  }
  XtVaGetValues(appShell, XmNcolormap, &colormap, NULL);
  if (resources.paperColor) {
    if (XAllocNamedColor(display, colormap, resources.paperColor,
			 &xcol, &xcol2)) {
      paperRGB[0] = xcol.red >> 8;
      paperRGB[1] = xcol.green >> 8;
      paperRGB[2] = xcol.blue >> 8;
      paperPixel = xcol.pixel;
    } else {
      error(errIO, -1, "Couldn't allocate color '{0:s}'",
	    resources.paperColor);
    }
  }
  if (XAllocNamedColor(display, colormap, resources.matteColor,
		       &xcol, &xcol2)) {
    mattePixel = xcol.pixel;
  } else {
    mattePixel = paperPixel;
  }
  if (XAllocNamedColor(display, colormap, resources.fullScreenMatteColor,
		       &xcol, &xcol2)) {
    fullScreenMattePixel = xcol.pixel;
  } else {
    fullScreenMattePixel = paperPixel;
  }
  initialZoom = resources.initialZoom ? resources.initialZoom : "";
}

XPDFApp::~XPDFApp() {
  // Empty because XPDFViewer is a forward declaration in the header
}

XPDFViewer *XPDFApp::open(const std::string *fileName, int page,
			  const std::string *dest,
			  const std::string *ownerPassword,
			  const std::string *userPassword) {
  XPDFViewer *viewer;

  viewer = new XPDFViewer(this, fileName, page, dest, fullScreen,
			  ownerPassword, userPassword);
  if (!viewer->isOk()) {
    delete viewer;
    return NULL;
  }
  if (remoteAtom != None) {
    remoteViewer = viewer;
    remoteWin = viewer->getWindow();
    XtAddEventHandler(remoteWin, PropertyChangeMask, False,
		      &remoteMsgCbk, this);
    XSetSelectionOwner(display, remoteAtom, XtWindow(remoteWin), CurrentTime);
  }
  viewers.emplace_back(viewer);
  return viewer;
}

XPDFViewer *XPDFApp::reopen(XPDFViewer *viewer, PDFDoc *doc, int page,
			    bool fullScreenA) {
  for (auto it = viewers.begin(); it != viewers.end(); it++) {
    if (it->get() == viewer) {
      viewers.erase(it);
      break;
    }
  }
  viewer = new XPDFViewer(this, doc, page, NULL, fullScreenA);
  if (!viewer->isOk()) {
    delete viewer;
    return NULL;
  }
  if (remoteAtom != None) {
    remoteViewer = viewer;
    remoteWin = viewer->getWindow();
    XtAddEventHandler(remoteWin, PropertyChangeMask, False,
		      &remoteMsgCbk, this);
    XSetSelectionOwner(display, remoteAtom, XtWindow(remoteWin), CurrentTime);
  }
  viewers.emplace_back(viewer);
  return viewer;
}

void XPDFApp::close(XPDFViewer *viewer, bool closeLast) {
  if (viewers.size() == 1) {
    if (viewer != viewers.front().get()) {
      return;
    }
    if (closeLast) {
      quit();
    } else {
      viewer->clear();
    }
  } else {
    for (auto it = viewers.begin(); it != viewers.end(); it++) {
      if (it->get() == viewer) {
	bool wasRemote = remoteViewer == viewer;
	viewers.erase(it);
	if (remoteAtom != None && wasRemote) {
	  remoteViewer = viewers.back().get();
	  remoteWin = remoteViewer->getWindow();
	  XSetSelectionOwner(display, remoteAtom, XtWindow(remoteWin),
			     CurrentTime);
	}
	return;
      }
    }
  }
}

void XPDFApp::quit() {
  if (remoteAtom != None) {
    XSetSelectionOwner(display, remoteAtom, None, CurrentTime);
  }
  viewers.clear();
#if HAVE_XTAPPSETEXITFLAG
  XtAppSetExitFlag(appContext);
#else
  exit(0);
#endif
}

void XPDFApp::run() {
  XtAppMainLoop(appContext);
}

void XPDFApp::setRemoteName(char *remoteName) {
  remoteAtom = XInternAtom(display, remoteName, False);
  remoteXWin = XGetSelectionOwner(display, remoteAtom);
}

bool XPDFApp::remoteServerRunning() {
  return remoteXWin != None;
}

void XPDFApp::remoteSend(const std::string& cmd) {
  if (cmd.size() + 1 > remoteCmdSize) {
    error(errCommandLine, -1, "Remote command is too long");
    return;
  }

  XChangeProperty(display, remoteXWin, remoteAtom, remoteAtom, 8,
		  PropModeReplace, (unsigned char *)cmd.c_str(),
		  cmd.size() + 1);
  XFlush(display);
}

void XPDFApp::remoteExec(const std::string& cmd) {
  remoteSend(cmd + "\n");
}

void XPDFApp::remoteOpen(const std::string& fileName, int page, bool raise) {
  std::string cmd;

  cmd = "openFileAtPage(" + fileName + "," + std::to_string(page) + ")\n";
  if (raise) {
    cmd += "raise\n";
  }
  remoteSend(cmd);
}

void XPDFApp::remoteOpenAtDest(const std::string& fileName,
			       const std::string& dest, bool raise) {
  std::string cmd;

  cmd = "openFileAtDest(" + fileName + "," + dest + ")\n";
  if (raise) {
    cmd += "raise\n";
  }
  remoteSend(cmd);
}

void XPDFApp::remoteReload(bool raise) {
  std::string cmd;

  cmd = "reload\n";
  if (raise) {
    cmd += "raise\n";
  }
  remoteSend(cmd);
}

void XPDFApp::remoteRaise() {
  remoteSend("raise\n");
}

void XPDFApp::remoteQuit() {
  remoteSend("quit\n");
}

void XPDFApp::remoteMsgCbk(Widget widget, XtPointer ptr,
			   XEvent *event, Boolean *cont) {
  XPDFApp *app = (XPDFApp *)ptr;
  char *cmd, *p0, *p1;
  Atom type;
  int format;
  unsigned long size, remain;

  if (event->xproperty.atom != app->remoteAtom) {
    *cont = True;
    return;
  }
  *cont = False;

  if (XGetWindowProperty(app->display, XtWindow(app->remoteWin),
			 app->remoteAtom, 0, remoteCmdSize/4,
			 True, app->remoteAtom,
			 &type, &format, &size, &remain,
			 (unsigned char **)&cmd) != Success) {
    return;
  }
  if (!cmd) {
    return;
  }
  p0 = cmd;
  while (*p0 && (p1 = strchr(p0, '\n'))) {
    std::string cmdStr(p0, p1 - p0);
    app->remoteViewer->execCmd(cmdStr, NULL);
    p0 = p1 + 1;
  }
  XFree((XPointer)cmd);
}
