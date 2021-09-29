//========================================================================
//
// XPDFViewer.cc
//
// Copyright 2002-2003 Glyph & Cog, LLC
// Copyright 2014-2020 Adam Sampson <ats@offog.org>
//
// Modified for Debian by Hamish Moffatt, 22 May 2002.
//
//========================================================================

#include <poppler-config.h>

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>
#ifdef HAVE_X11_XPM_H
#include <X11/xpm.h>
#endif
#if defined(__sgi) && (XmVERSION <= 1)
#define Object XtObject
#include <Sgm/HPanedW.h>
#undef Object
#endif
#include <goo/gmem.h>
#include <goo/gfile.h>
#include "gfile-xpdf.h"
#include <goo/GooString.h>
#ifndef NO_GOOLIST
#include <goo/GooList.h>
#endif
#include "Error.h"
#include "GlobalParams.h"
#include "XPDFParams.h"
#include "PDFDoc.h"
#include "Link.h"
#include "ErrorCodes.h"
#include "Outline.h"
#include "UnicodeMap.h"
#ifndef DISABLE_OUTLINE
#define Object XtObject
#include "XPDFTree.h"
#undef Object
#endif
#include "XPDFApp.h"
#include "XPDFViewer.h"
#include "PSOutputDev.h"
#include "config.h"

// these macro defns conflict with xpdf's Object class
#ifdef LESSTIF_VERSION
#undef XtDisplay
#undef XtScreen
#undef XtWindow
#undef XtParent
#undef XtIsRealized
#endif

#if XmVERSION <= 1
#define XmSET   True
#define XmUNSET False
#endif

// hack around old X includes which are missing these symbols
#ifndef XK_Page_Up
#define XK_Page_Up              0xFF55
#endif
#ifndef XK_Page_Down
#define XK_Page_Down            0xFF56
#endif
#ifndef XK_KP_Home
#define XK_KP_Home              0xFF95
#endif
#ifndef XK_KP_Left
#define XK_KP_Left              0xFF96
#endif
#ifndef XK_KP_Up
#define XK_KP_Up                0xFF97
#endif
#ifndef XK_KP_Right
#define XK_KP_Right             0xFF98
#endif
#ifndef XK_KP_Down
#define XK_KP_Down              0xFF99
#endif
#ifndef XK_KP_Page_Up
#define XK_KP_Page_Up           0xFF9A
#endif
#ifndef XK_KP_Page_Down
#define XK_KP_Page_Down         0xFF9B
#endif
#ifndef XK_KP_End
#define XK_KP_End               0xFF9C
#endif
#ifndef XK_KP_Insert
#define XK_KP_Insert            0xFF9E
#endif
#ifndef XK_KP_Delete
#define XK_KP_Delete            0xFF9F
#endif

//------------------------------------------------------------------------
// GUI includes
//------------------------------------------------------------------------

#include "leftArrow.xbm"
#include "leftArrowDis.xbm"
#include "dblLeftArrow.xbm"
#include "dblLeftArrowDis.xbm"
#include "rightArrow.xbm"
#include "rightArrowDis.xbm"
#include "dblRightArrow.xbm"
#include "dblRightArrowDis.xbm"
#include "backArrow.xbm"
#include "backArrowDis.xbm"
#include "forwardArrow.xbm"
#include "forwardArrowDis.xbm"
#include "find.xbm"
#include "findDis.xbm"
#include "print.xbm"
#include "printDis.xbm"
#include "about.xbm"
#include "about-text.h"

//------------------------------------------------------------------------

struct ZoomMenuInfo {
  char *label;
  double zoom;
};

static ZoomMenuInfo zoomMenuInfo[] = {
  { "1600%",    1600 },
  { "1200%",    1200 },
  { "800%",      800 },
  { "600%",      600 },
  { "400%",      400 },
  { "200%",      200 },
  { "150%",      150 },
  { "125%",      125 },
  { "100%",      100 },
  { "50%",        50 },
  { "25%",        25 },
  { "12.5%",      12.5 },
  { "fit page",  zoomPage },
  { "fit width", zoomWidth },
  { "fit height", zoomHeight }
};

static const int nZoomMenuItems = (sizeof(zoomMenuInfo)/sizeof(struct ZoomMenuInfo));

#ifndef USE_COMBO_BOX
  Widget zoomMenuBtns[nZoomMenuItems];
#endif

static const int maxZoomIdx = 0;
static const int minZoomIdx = nZoomMenuItems - 4;
static const int zoomPageIdx = nZoomMenuItems - 3;
static const int zoomWidthIdx = nZoomMenuItems - 2;
static const int zoomHeightIdx = nZoomMenuItems -1;

//------------------------------------------------------------------------

XPDFViewerCmd XPDFViewer::cmdTab[] = {
  { "about",                   0, false, false, &XPDFViewer::cmdAbout },
  { "closeOutline",            0, false, false, &XPDFViewer::cmdCloseOutline },
  { "closeWindow",             0, false, false, &XPDFViewer::cmdCloseWindow },
  { "closeWindowOrQuit",       0, false, false, &XPDFViewer::cmdCloseWindowOrQuit },
  { "continuousMode",          0, false, false, &XPDFViewer::cmdContinuousMode },
  { "endPan",                  0, true,  true,  &XPDFViewer::cmdEndPan },
  { "endSelection",            0, true,  true,  &XPDFViewer::cmdEndSelection },
  { "find",                    0, true,  false, &XPDFViewer::cmdFind },
  { "findNext",                0, true,  false, &XPDFViewer::cmdFindNext },
  { "findPrev",                0, true,  false, &XPDFViewer::cmdFindPrev },
  { "focusToDocWin",           0, false, false, &XPDFViewer::cmdFocusToDocWin },
  { "focusToPageNum",          0, false, false, &XPDFViewer::cmdFocusToPageNum },
  { "followLink",              0, true,  true,  &XPDFViewer::cmdFollowLink },
  { "followLinkInNewWin",      0, true,  true,  &XPDFViewer::cmdFollowLinkInNewWin },
  { "followLinkInNewWinNoSel", 0, true,  true,  &XPDFViewer::cmdFollowLinkInNewWinNoSel },
  { "followLinkNoSel",         0, true,  true,  &XPDFViewer::cmdFollowLinkNoSel },
  { "fullScreenMode",          0, false, false, &XPDFViewer::cmdFullScreenMode },
  { "goBackward",              0, false, false, &XPDFViewer::cmdGoBackward },
  { "goForward",               0, false, false, &XPDFViewer::cmdGoForward },
  { "gotoDest",                1, true,  false, &XPDFViewer::cmdGotoDest },
  { "gotoLastPage",            0, true,  false, &XPDFViewer::cmdGotoLastPage },
  { "gotoLastPageNoScroll",    0, true,  false, &XPDFViewer::cmdGotoLastPageNoScroll },
  { "gotoPage",                1, true,  false, &XPDFViewer::cmdGotoPage },
  { "gotoPageNoScroll",        1, true,  false, &XPDFViewer::cmdGotoPageNoScroll },
  { "nextPage",                0, true,  false, &XPDFViewer::cmdNextPage },
  { "nextPageNoScroll",        0, true,  false, &XPDFViewer::cmdNextPageNoScroll },
  { "open",                    0, false, false, &XPDFViewer::cmdOpen },
  { "openFile",                1, false, false, &XPDFViewer::cmdOpenFile },
  { "openFileAtDest",          2, false, false, &XPDFViewer::cmdOpenFileAtDest },
  { "openFileAtDestInNewWin",  2, false, false, &XPDFViewer::cmdOpenFileAtDestInNewWin },
  { "openFileAtPage",          2, false, false, &XPDFViewer::cmdOpenFileAtPage },
  { "openFileAtPageInNewWin",  2, false, false, &XPDFViewer::cmdOpenFileAtPageInNewWin },
  { "openFileInNewWin",        1, false, false, &XPDFViewer::cmdOpenFileInNewWin },
  { "openInNewWin",            0, false, false, &XPDFViewer::cmdOpenInNewWin },
  { "openOutline",             0, false, false, &XPDFViewer::cmdOpenOutline },
  { "pageDown",                0, true,  false, &XPDFViewer::cmdPageDown },
  { "pageUp",                  0, true,  false, &XPDFViewer::cmdPageUp },
  { "postPopupMenu",           0, false, true,  &XPDFViewer::cmdPostPopupMenu },
  { "prevPage",                0, true,  false, &XPDFViewer::cmdPrevPage },
  { "prevPageNoScroll",        0, true,  false, &XPDFViewer::cmdPrevPageNoScroll },
  { "print",                   0, true,  false, &XPDFViewer::cmdPrint },
  { "quit",                    0, false, false, &XPDFViewer::cmdQuit },
  { "raise",                   0, false, false, &XPDFViewer::cmdRaise },
  { "redraw",                  0, true,  false, &XPDFViewer::cmdRedraw },
  { "reload",                  0, true,  false, &XPDFViewer::cmdReload },
  { "rotateCCW",               0, true,  false, &XPDFViewer::cmdRotateCCW },
  { "rotateCW",                0, true,  false, &XPDFViewer::cmdRotateCW },
  { "run",                     1, false, false, &XPDFViewer::cmdRun },
  { "saveAs",                  0, true,  false, &XPDFViewer::cmdSaveAs },
  { "scrollDown",              1, true,  false, &XPDFViewer::cmdScrollDown },
  { "scrollDownNextPage",      1, true,  false, &XPDFViewer::cmdScrollDownNextPage },
  { "scrollLeft",              1, true,  false, &XPDFViewer::cmdScrollLeft },
  { "scrollOutlineDown",       1, true,  false, &XPDFViewer::cmdScrollOutlineDown },
  { "scrollOutlineUp",         1, true,  false, &XPDFViewer::cmdScrollOutlineUp },
  { "scrollRight",             1, true,  false, &XPDFViewer::cmdScrollRight },
  { "scrollToBottomEdge",      0, true,  false, &XPDFViewer::cmdScrollToBottomEdge },
  { "scrollToBottomRight",     0, true,  false, &XPDFViewer::cmdScrollToBottomRight },
  { "scrollToLeftEdge",        0, true,  false, &XPDFViewer::cmdScrollToLeftEdge },
  { "scrollToRightEdge",       0, true,  false, &XPDFViewer::cmdScrollToRightEdge },
  { "scrollToTopEdge",         0, true,  false, &XPDFViewer::cmdScrollToTopEdge },
  { "scrollToTopLeft",         0, true,  false, &XPDFViewer::cmdScrollToTopLeft },
  { "scrollUp",                1, true,  false, &XPDFViewer::cmdScrollUp },
  { "scrollUpPrevPage",        1, true,  false, &XPDFViewer::cmdScrollUpPrevPage },
  { "search",                  1, true,  false, &XPDFViewer::cmdSearch },
  { "setSelection",            5, true,  false, &XPDFViewer::cmdSetSelection },
  { "singlePageMode",          0, false, false, &XPDFViewer::cmdSinglePageMode },
  { "startPan",                0, true,  true,  &XPDFViewer::cmdStartPan },
  { "startSelection",          0, true,  true,  &XPDFViewer::cmdStartSelection },
  { "toggleContinuousMode",    0, false, false, &XPDFViewer::cmdToggleContinuousMode },
  { "toggleFullScreenMode",    0, false, false, &XPDFViewer::cmdToggleFullScreenMode },
  { "toggleOutline",           0, false, false, &XPDFViewer::cmdToggleOutline },
  { "windowMode",              0, false, false, &XPDFViewer::cmdWindowMode },
  { "zoomFitHeight",           0, false, false, &XPDFViewer::cmdZoomFitHeight },
  { "zoomFitPage",             0, false, false, &XPDFViewer::cmdZoomFitPage },
  { "zoomFitWidth",            0, false, false, &XPDFViewer::cmdZoomFitWidth },
  { "zoomIn",                  0, false, false, &XPDFViewer::cmdZoomIn },
  { "zoomOut",                 0, false, false, &XPDFViewer::cmdZoomOut },
  { "zoomPercent",             1, false, false, &XPDFViewer::cmdZoomPercent },
  { "zoomToSelection",         0, true,  false, &XPDFViewer::cmdZoomToSelection }
};

#define nCmds (sizeof(cmdTab) / sizeof(XPDFViewerCmd))

//------------------------------------------------------------------------

XPDFViewer::XPDFViewer(XPDFApp *appA, const std::string *fileName,
		       int pageA, const std::string *destName, bool fullScreen,
		       const std::string *ownerPassword,
		       const std::string *userPassword) {
  init(appA, NULL, fileName, pageA, destName, fullScreen,
       ownerPassword, userPassword);
}

XPDFViewer::XPDFViewer(XPDFApp *appA, PDFDoc *doc, int pageA,
		       const std::string *destName, bool fullScreen) {
  init(appA, doc, NULL, pageA, destName, fullScreen, NULL, NULL);
}

void XPDFViewer::init(XPDFApp *appA, PDFDoc *doc, const std::string *fileName,
		      int pageA, const std::string *destName, bool fullScreen,
		      const std::string *ownerPassword,
		      const std::string *userPassword) {
  std::unique_ptr<LinkDest> dest;
  int pg = pageA;
  double z;

  app = appA;
  win = NULL;
  core = NULL;
  ok = false;
#ifndef DISABLE_OUTLINE
  outlineLabels = NULL;
  outlineLabelsLength = outlineLabelsSize = 0;
  outlinePaneWidth = 175;
#endif

  // do Motif-specific initialization and create the window;
  // this also creates the core object
  initWindow(fullScreen);
  initAboutDialog();
  initFindDialog();
  initPrintDialog();
  openDialog = NULL;
  saveAsDialog = NULL;

  if (doc || fileName) {
    if (doc) {
      core->loadDoc(doc);
    } else if (!loadFile(*fileName, ownerPassword, userPassword)) {
      return;
    }
    getPageAndDest(pageA, destName, pg, dest);
#ifndef DISABLE_OUTLINE
    if (outlineScroll != None &&
	core->getDoc()->getOutline()->getItems() &&
	core->getDoc()->getOutline()->getItems()->getOILSize() > 0) {
      XtVaSetValues(outlineScroll, XmNwidth, outlinePaneWidth, NULL);
    }
#endif
  }
  core->resizeToPage(pg);

  // map the window -- we do this after calling resizeToPage to avoid
  // an annoying on-screen resize
  mapWindow();

  // display the first page
  z = core->getZoom();
  if (dest) {
    displayDest(dest, z, core->getRotate(), true);
  } else {
    displayPage(pg, z, core->getRotate(), true, true);
  }

  ok = true;
}

XPDFViewer::~XPDFViewer() {
  delete core;
  if (aboutBigFont) {
    XmFontListFree(aboutBigFont);
  }
  if (aboutVersionFont) {
    XmFontListFree(aboutVersionFont);
  }
  if (aboutFixedFont) {
    XmFontListFree(aboutFixedFont);
  }
  closeWindow();
#ifndef DISABLE_OUTLINE
  if (outlineLabels) {
    gfree(outlineLabels);
  }
#endif
}

void XPDFViewer::open(const std::string& fileName, int pageA,
		      const std::string *destName) {
  std::unique_ptr<LinkDest> dest;
  int pg;
  double z;

  if (!core->getDoc() ||
      !core->getDoc()->getFileName() ||
      fileName != toString(core->getDoc()->getFileName())) {
    if (!loadFile(fileName, NULL, NULL)) {
      return;
    }
  }
  getPageAndDest(pageA, destName, pg, dest);
  z = core->getZoom();
  if (dest) {
    displayDest(dest, z, core->getRotate(), true);
  } else {
    displayPage(pg, z, core->getRotate(), true, true);
  }
}

void XPDFViewer::clear() {
  const char *title;
  XmString s;

  core->clear();

  // set up title
  title = app->getTitle() != "" ? app->getTitle().c_str()
				: xpdfAppName;
  XtVaSetValues(win, XmNtitle, (char *)title, XmNiconName, title, NULL);

  if (toolBar != None) {

    // set up number-of-pages display
    s = XmStringCreateLocalized("");
    XtVaSetValues(pageNumText, XmNlabelString, s, NULL);
    XmStringFree(s);
    s = XmStringCreateLocalized(" of 0");
    XtVaSetValues(pageCountLabel, XmNlabelString, s, NULL);
    XmStringFree(s);

    // disable buttons
    XtVaSetValues(prevTenPageBtn, XmNsensitive, False, NULL);
    XtVaSetValues(prevPageBtn, XmNsensitive, False, NULL);
    XtVaSetValues(nextTenPageBtn, XmNsensitive, False, NULL);
    XtVaSetValues(nextPageBtn, XmNsensitive, False, NULL);
  }

  // remove the old outline
#ifndef DISABLE_OUTLINE
  setupOutline();
#endif
}

//------------------------------------------------------------------------
// load / display
//------------------------------------------------------------------------

bool XPDFViewer::loadFile(const std::string& fileName,
			  const std::string *ownerPassword,
			  const std::string *userPassword) {
  return core->loadFile(fileName, ownerPassword, userPassword) == errNone;
}

void XPDFViewer::reloadFile() {
  int pg;

  if (!core->getDoc() || !core->getDoc()->getFileName()) {
    return;
  }
  pg = core->getPageNum();
  loadFile(toString(core->getDoc()->getFileName()));
  if (pg > core->getDoc()->getNumPages()) {
    pg = core->getDoc()->getNumPages();
  }
  displayPage(pg, core->getZoom(), core->getRotate(), false, false);
}

void XPDFViewer::displayPage(int pageA, double zoomA, int rotateA,
			     bool scrollToTop, bool addToHist) {
  core->displayPage(pageA, zoomA, rotateA, scrollToTop, addToHist);
}

void XPDFViewer::displayDest(const std::unique_ptr<LinkDest> &dest, double zoomA,
			     int rotateA, bool addToHist) {
  core->displayDest(dest.get(), zoomA, rotateA, addToHist);
}

void XPDFViewer::getPageAndDest(int pageA, const std::string *destName,
				int &pageOut, std::unique_ptr<LinkDest> &destOut) {
  Ref pageRef;

  // find the page number for a named destination
  pageOut = pageA;
  destOut.reset();

  if (destName) {
    auto destGS = makeGooStringPtr(destName);
    std::unique_ptr<LinkDest> dest(core->getDoc()->findDest(destGS.get()));
    if (dest) {
      destOut = std::move(dest);

      if (destOut->isPageRef()) {
        pageRef = destOut->getPageRef();
#ifdef FINDPAGE_REF
        pageOut = core->getDoc()->findPage(pageRef);
#else
        pageOut = core->getDoc()->findPage(pageRef.num, pageRef.gen);
#endif
      } else {
        pageOut = destOut->getPageNum();
      }
    }
  }

  if (pageOut <= 0) {
    pageOut = 1;
  }
  if (pageOut > core->getDoc()->getNumPages()) {
    pageOut = core->getDoc()->getNumPages();
  }
}

//------------------------------------------------------------------------
// hyperlinks / actions
//------------------------------------------------------------------------

void XPDFViewer::doLink(int wx, int wy, bool onlyIfNoSelection,
			bool newWin) {
  XPDFViewer *newViewer;
  LinkAction *action;
  int pg, selPg;
  double xu, yu, selULX, selULY, selLRX, selLRY;

  if (core->getHyperlinksEnabled() &&
      core->cvtWindowToUser(wx, wy, &pg, &xu, &yu) &&
      !(onlyIfNoSelection &&
	core->getSelection(&selPg, &selULX, &selULY, &selLRX, &selLRY))) {
    if ((action = core->findLink(pg, xu, yu))) {
      if (newWin &&
	  core->getDoc()->getFileName() &&
	  (action->getKind() == actionGoTo ||
	   action->getKind() == actionGoToR ||
	   (action->getKind() == actionNamed &&
	    toString(((LinkNamed *)action)->getName()) == "Quit"))) {
	auto fileName = toString(core->getDoc()->getFileName());
	newViewer = app->open(&fileName);
	newViewer->core->doAction(action);
      } else {
	core->doAction(action);
      }
    }
  }
}

void XPDFViewer::actionCbk(void *data, const char *action) {
  XPDFViewer *viewer = (XPDFViewer *)data;

  if (!strcmp(action, "Quit")) {
    viewer->app->quit();
  }
}

//------------------------------------------------------------------------
// keyboard/mouse input
//------------------------------------------------------------------------

void XPDFViewer::keyPressCbk(void *data, KeySym key, unsigned int modifiers,
			     XEvent *event) {
  XPDFViewer *viewer = (XPDFViewer *)data;
  int keyCode;

  if (key >= 0x20 && key <= 0xfe) {
    keyCode = (int)key;
  } else if (key == XK_Tab ||
	     key == XK_KP_Tab) {
    keyCode = xpdfKeyCodeTab;
  } else if (key == XK_Return) {
    keyCode = xpdfKeyCodeReturn;
  } else if (key == XK_KP_Enter) {
    keyCode = xpdfKeyCodeEnter;
  } else if (key == XK_BackSpace) {
    keyCode = xpdfKeyCodeBackspace;
  } else if (key == XK_Insert ||
	     key == XK_KP_Insert) {
    keyCode = xpdfKeyCodeInsert;
  } else if (key == XK_Delete ||
	     key == XK_KP_Delete) {
    keyCode = xpdfKeyCodeDelete;
  } else if (key == XK_Home ||
	     key == XK_KP_Home) {
    keyCode = xpdfKeyCodeHome;
  } else if (key == XK_End ||
	     key == XK_KP_End) {
    keyCode = xpdfKeyCodeEnd;
  } else if (key == XK_Page_Up ||
	     key == XK_KP_Page_Up) {
    keyCode = xpdfKeyCodePgUp;
  } else if (key == XK_Page_Down ||
	     key == XK_KP_Page_Down) {
    keyCode = xpdfKeyCodePgDn;
  } else if (key == XK_Left ||
	     key == XK_KP_Left) {
    keyCode = xpdfKeyCodeLeft;
  } else if (key == XK_Right ||
	     key == XK_KP_Right) {
    keyCode = xpdfKeyCodeRight;
  } else if (key == XK_Up ||
	     key == XK_KP_Up) {
    keyCode = xpdfKeyCodeUp;
  } else if (key == XK_Down ||
	     key == XK_KP_Down) {
    keyCode = xpdfKeyCodeDown;
  } else if (key == XK_Escape) {
    keyCode = xpdfKeyCodeEscape;
  } else if (key >= XK_F1 && key <= XK_F35) {
    keyCode = xpdfKeyCodeF1 + (key - XK_F1);
  } else if (key == XK_KP_Add) {
    keyCode = '+';
  } else if (key == XK_KP_Subtract) {
    keyCode = '-';
  } else {
    return;
  }

  for (const auto& cmd: xpdfParams->getKeyBinding(
				keyCode,
				viewer->getModifiers(modifiers),
				viewer->getContext(modifiers))) {
    viewer->execCmd(cmd, event);
  }
}

void XPDFViewer::mouseCbk(void *data, XEvent *event) {
  XPDFViewer *viewer = (XPDFViewer *)data;
  int keyCode;

  if (event->type == ButtonPress) {
    if (event->xbutton.button >= 1 && event->xbutton.button <= 32) {
      keyCode = xpdfKeyCodeMousePress1 + event->xbutton.button - 1;
    } else {
      return;
    }
  } else if (event->type == ButtonRelease) {
    if (event->xbutton.button >= 1 && event->xbutton.button <= 32) {
      keyCode = xpdfKeyCodeMouseRelease1 + event->xbutton.button - 1;
    } else {
      return;
    }
  } else {
    return;
  }

  for (const auto& cmd: xpdfParams->getKeyBinding(
					keyCode,
					viewer->getModifiers(
						event->xkey.state),
					viewer->getContext(
						event->xkey.state))) {
    viewer->execCmd(cmd, event);
  }
}

int XPDFViewer::getModifiers(unsigned int modifiers) {
  int mods;

  mods = 0;
  if (modifiers & ShiftMask) {
    mods |= xpdfKeyModShift;
  }
  if (modifiers & ControlMask) {
    mods |= xpdfKeyModCtrl;
  }
  if (modifiers & Mod1Mask) {
    mods |= xpdfKeyModAlt;
  }
  return mods;
}

int XPDFViewer::getContext(unsigned int modifiers) {
  int context;

  context = (core->getFullScreen() ? xpdfKeyContextFullScreen
                                   : xpdfKeyContextWindow) |
            (core->getContinuousMode() ? xpdfKeyContextContinuous
                                       : xpdfKeyContextSinglePage) |
            (core->getLinkAction() ? xpdfKeyContextOverLink
                                   : xpdfKeyContextOffLink) |
            ((modifiers & Mod5Mask) ? xpdfKeyContextScrLockOn
	                            : xpdfKeyContextScrLockOff);
  return context;
}

void XPDFViewer::execCmd(const std::string& cmd, XEvent *event) {
  std::string name;
  CmdList args;
  const char *p0, *p1;
  int a, b, m, cmp;

  //----- parse the command
  p0 = cmd.c_str();
  for (p1 = p0; *p1 && isalnum(*p1); ++p1) ;
  if (p1 == p0) {
    goto err1;
  }
  name = std::string(p0, p1 - p0);
  if (*p1 == '(') {
    while (true) {
      p0 = p1 + 1;
      for (p1 = p0; *p1 && *p1 != ',' && *p1 != ')'; ++p1) ;
      args.emplace_back(p0, p1 - p0);
      if (*p1 != ',') {
	break;
      }
    }
    if (*p1 != ')') {
      goto err1;
    }
    ++p1;
  }
  if (*p1) {
    goto err1;
  }

  //----- find the command
  a = -1;
  b = nCmds;
  // invariant: cmdTab[a].name < name < cmdTab[b].name
  while (b - a > 1) {
    m = (a + b) / 2;
    cmp = strcmp(cmdTab[m].name, name.c_str());
    if (cmp < 0) {
      a = m;
    } else if (cmp > 0) {
      b = m;
    } else {
      a = b = m;
    }
  }
  if (cmp != 0) {
    goto err1;
  }

  //----- execute the command
  if (args.size() != cmdTab[a].nArgs ||
      (cmdTab[a].requiresEvent && !event)) {
    goto err1;
  }
  if (cmdTab[a].requiresDoc && !core->getDoc()) {
    // don't issue an error message for this -- it happens, e.g., when
    // clicking in a window with no open PDF file
    return;
  }
  (this->*cmdTab[a].func)(args, event);

  return;

 err1:
  error(errConfig, -1, "Invalid command syntax: '{0:s}'", cmd.c_str());
}

//------------------------------------------------------------------------
// command functions
//------------------------------------------------------------------------

static int mouseX(XEvent *event) {
  switch (event->type) {
  case ButtonPress:
  case ButtonRelease:
    return event->xbutton.x;
  case KeyPress:
    return event->xkey.x;
  }
  return 0;
}

static int mouseY(XEvent *event) {
  switch (event->type) {
  case ButtonPress:
  case ButtonRelease:
    return event->xbutton.y;
  case KeyPress:
    return event->xkey.y;
  }
  return 0;
}

void XPDFViewer::cmdAbout(const CmdList& args, XEvent *event) {
  XtManageChild(aboutDialog);
}

void XPDFViewer::cmdCloseOutline(const CmdList& args, XEvent *event) {
#ifndef DISABLE_OUTLINE
  Dimension w;

  if (outlineScroll == None) {
    return;
  }
  XtVaGetValues(outlineScroll, XmNwidth, &w, NULL);
  if (w > 1) {
    outlinePaneWidth = w;
    // this ugly kludge is apparently the only way to resize the panes
    // within an XmPanedWindow
    XtVaSetValues(outlineScroll, XmNpaneMinimum, 1,
		  XmNpaneMaximum, 1, NULL);
    XtVaSetValues(outlineScroll, XmNpaneMinimum, 1,
		  XmNpaneMaximum, 10000, NULL);
  }
#endif
}

void XPDFViewer::cmdCloseWindow(const CmdList& args, XEvent *event) {
  app->close(this, false);
}

void XPDFViewer::cmdCloseWindowOrQuit(const CmdList& args, XEvent *event) {
  app->close(this, true);
}

void XPDFViewer::cmdContinuousMode(const CmdList& args, XEvent *event) {
  Widget btn;

  if (core->getContinuousMode()) {
    return;
  }
  core->setContinuousMode(true);

  btn = XtNameToWidget(popupMenu, "continuousMode");
  XtVaSetValues(btn, XmNset, XmSET, NULL);
}

void XPDFViewer::cmdEndPan(const CmdList& args, XEvent *event) {
  core->endPan(mouseX(event), mouseY(event));
}

void XPDFViewer::cmdEndSelection(const CmdList& args, XEvent *event) {
  core->endSelection(mouseX(event), mouseY(event));
}

void XPDFViewer::cmdFind(const CmdList& args, XEvent *event) {
  mapFindDialog();
}

void XPDFViewer::cmdFindPrev(const CmdList& args, XEvent *event) {
  doFind(true, 1);
}

void XPDFViewer::cmdFindNext(const CmdList& args, XEvent *event) {
  doFind(true, 0);
}

void XPDFViewer::cmdFocusToDocWin(const CmdList& args, XEvent *event) {
  core->takeFocus();
}

void XPDFViewer::cmdFocusToPageNum(const CmdList& args, XEvent *event) {
  if (toolBar != None) {
    XmTextFieldSetSelection(pageNumText, 0,
			    strlen(XmTextFieldGetString(pageNumText)),
			    XtLastTimestampProcessed(display));
    XmProcessTraversal(pageNumText, XmTRAVERSE_CURRENT);
  }
}

void XPDFViewer::cmdFollowLink(const CmdList& args, XEvent *event) {
  doLink(mouseX(event), mouseY(event), false, false);
}

void XPDFViewer::cmdFollowLinkInNewWin(const CmdList& args, XEvent *event) {
  doLink(mouseX(event), mouseY(event), false, true);
}

void XPDFViewer::cmdFollowLinkInNewWinNoSel(const CmdList& args,
					    XEvent *event) {
  doLink(mouseX(event), mouseY(event), true, true);
}

void XPDFViewer::cmdFollowLinkNoSel(const CmdList& args, XEvent *event) {
  doLink(mouseX(event), mouseY(event), true, false);
}

void XPDFViewer::cmdFullScreenMode(const CmdList& args, XEvent *event) {
  PDFDoc *doc;
  XPDFViewer *viewer;
  int pg;
  Widget btn;

  if (core->getFullScreen()) {
    return;
  }
  pg = core->getPageNum();
  XtPopdown(win);
  doc = core->takeDoc(false);
  viewer = app->reopen(this, doc, pg, true);

  btn = XtNameToWidget(viewer->popupMenu, "fullScreen");
  XtVaSetValues(btn, XmNset, XmSET, NULL);
}

void XPDFViewer::cmdGoBackward(const CmdList& args, XEvent *event) {
  core->goBackward();
}

void XPDFViewer::cmdGoForward(const CmdList& args, XEvent *event) {
  core->goForward();
}

void XPDFViewer::cmdGotoDest(const CmdList& args, XEvent *event) {
  int pg;
  std::unique_ptr<LinkDest> dest;

  getPageAndDest(1, &args[0], pg, dest);
  if (dest) {
    displayDest(dest, core->getZoom(), core->getRotate(), true);
  }
}

void XPDFViewer::cmdGotoLastPage(const CmdList& args, XEvent *event) {
  displayPage(core->getDoc()->getNumPages(),
	      core->getZoom(), core->getRotate(),
	      true, true);
}

void XPDFViewer::cmdGotoLastPageNoScroll(const CmdList& args, XEvent *event) {
  displayPage(core->getDoc()->getNumPages(),
	      core->getZoom(), core->getRotate(),
	      false, true);
}

void XPDFViewer::cmdGotoPage(const CmdList& args, XEvent *event) {
  int pg;

  pg = atoi(args[0].c_str());
  if (pg < 1 || pg > core->getDoc()->getNumPages()) {
    return;
  }
  displayPage(pg, core->getZoom(), core->getRotate(), true, true);
}

void XPDFViewer::cmdGotoPageNoScroll(const CmdList& args, XEvent *event) {
  int pg;

  pg = atoi(args[0].c_str());
  if (pg < 1 || pg > core->getDoc()->getNumPages()) {
    return;
  }
  displayPage(pg, core->getZoom(), core->getRotate(), false, true);
}

void XPDFViewer::cmdNextPage(const CmdList& args, XEvent *event) {
  core->gotoNextPage(1, true);
}

void XPDFViewer::cmdNextPageNoScroll(const CmdList& args, XEvent *event) {
  core->gotoNextPage(1, false);
}

void XPDFViewer::cmdOpen(const CmdList& args, XEvent *event) {
  mapOpenDialog(false);
}

void XPDFViewer::cmdOpenFile(const CmdList& args, XEvent *event) {
  open(args[0], 1, NULL);
}

void XPDFViewer::cmdOpenFileAtDest(const CmdList& args, XEvent *event) {
  open(args[0], 1, &args[1]);
}

void XPDFViewer::cmdOpenFileAtDestInNewWin(const CmdList& args, XEvent *event) {
  app->open(&args[0], 1, &args[1]);
}

void XPDFViewer::cmdOpenFileAtPage(const CmdList& args, XEvent *event) {
  open(args[0], atoi(args[1].c_str()), NULL);
}

void XPDFViewer::cmdOpenFileAtPageInNewWin(const CmdList& args, XEvent *event) {
  app->open(&args[0], atoi(args[1].c_str()));
}

void XPDFViewer::cmdOpenFileInNewWin(const CmdList& args, XEvent *event) {
  app->open(&args[0]);
}

void XPDFViewer::cmdOpenInNewWin(const CmdList& args, XEvent *event) {
  mapOpenDialog(true);
}

void XPDFViewer::cmdOpenOutline(const CmdList& args, XEvent *event) {
#ifndef DISABLE_OUTLINE
  Dimension w;

  if (outlineScroll == None) {
    return;
  }
  XtVaGetValues(outlineScroll, XmNwidth, &w, NULL);
  if (w == 1) {
    // this ugly kludge is apparently the only way to resize the panes
    // within an XmPanedWindow
    XtVaSetValues(outlineScroll, XmNpaneMinimum, outlinePaneWidth,
		  XmNpaneMaximum, outlinePaneWidth, NULL);
    XtVaSetValues(outlineScroll, XmNpaneMinimum, 1,
		  XmNpaneMaximum, 10000, NULL);
  }
#endif
}

void XPDFViewer::cmdPageDown(const CmdList& args, XEvent *event) {
  core->scrollPageDown();
}

void XPDFViewer::cmdPageUp(const CmdList& args, XEvent *event) {
  core->scrollPageUp();
}

void XPDFViewer::cmdPostPopupMenu(const CmdList& args, XEvent *event) {
  XmMenuPosition(popupMenu, event->type == ButtonPress ? &event->xbutton
		                                       : (XButtonEvent *)NULL);
  XtManageChild(popupMenu);

  // this is magic (taken from DDD) - weird things happen if this
  // call isn't made (this is done in two different places, in hopes
  // of squashing this stupid bug)
  XtUngrabButton(core->getDrawAreaWidget(), AnyButton, AnyModifier);
}

void XPDFViewer::cmdPrevPage(const CmdList& args, XEvent *event) {
  core->gotoPrevPage(1, true, false);
}

void XPDFViewer::cmdPrevPageNoScroll(const CmdList& args, XEvent *event) {
  core->gotoPrevPage(1, false, false);
}

void XPDFViewer::cmdPrint(const CmdList& args, XEvent *event) {
  XtManageChild(printDialog);
}

void XPDFViewer::cmdQuit(const CmdList& args, XEvent *event) {
  app->quit();
}

void XPDFViewer::cmdRaise(const CmdList& args, XEvent *event) {
  XMapRaised(display, XtWindow(win));
  XFlush(display);
}

void XPDFViewer::cmdRedraw(const CmdList& args, XEvent *event) {
  displayPage(core->getPageNum(), core->getZoom(), core->getRotate(),
	      false, false);
}

void XPDFViewer::cmdReload(const CmdList& args, XEvent *event) {
  reloadFile();
}

void XPDFViewer::cmdRotateCCW(const CmdList& args, XEvent *event) {
  int r;

  r = core->getRotate();
  r = (r == 0) ? 270 : r - 90;
  displayPage(core->getPageNum(), core->getZoom(), r, true, false);
}

void XPDFViewer::cmdRotateCW(const CmdList& args, XEvent *event) {
  int r;

  r = core->getRotate();
  r = (r == 270) ? 0 : r + 90;
  displayPage(core->getPageNum(), core->getZoom(), r, true, false);
}

void XPDFViewer::cmdRun(const CmdList& args, XEvent *event) {
  const std::string& fmt = args[0];
  std::string cmd;
  const GooString *s;
  LinkAction *action;
  double selLRX, selLRY, selURX, selURY, mouseX, mouseY;
  int selPage, mousePage;
  bool gotSel, gotMouse;
  char buf[64];
  const char *p;
  char c0, c1;
  unsigned int i;
  int errcode;

  i = 0;
  gotSel = gotMouse = false;
  while (i < fmt.size()) {
    c0 = fmt[i];
    if (c0 == '%' && i + 1 < fmt.size()) {
      c1 = fmt[i + 1];
      switch (c1) {
      case 'f':
	if (core->getDoc() && (s = core->getDoc()->getFileName())) {
	  cmd.append(s->getCString());
	}
	break;
      case 'b':
	if (core->getDoc() && (s = core->getDoc()->getFileName())) {
	  if ((p = strrchr(s->getCString(), '.'))) {
	    cmd.append(s->getCString(), p - s->getCString());
	  } else {
	    cmd.append(s->getCString());
	  }
	}
	break;
      case 'u':
	if ((action = core->getLinkAction()) &&
	    action->getKind() == actionURI) {
	  cmd.append(core->mungeURL(toString(((LinkURI *)action)->getURI())));
	}
	break;
      case 'p':
	if (core->getDoc()) {
	  cmd.append(std::to_string(core->getPageNum()));
	}
	break;
      case 'x':
      case 'y':
      case 'X':
      case 'Y':
	if (!gotSel) {
	  if (!core->getSelection(&selPage, &selURX, &selURY,
				  &selLRX, &selLRY)) {
	    selPage = 0;
	    selURX = selURY = selLRX = selLRY = 0;
	  }
	  gotSel = true;
	}
	snprintf(buf, sizeof buf, "%g",
		 (c1 == 'x') ? selURX :
		 (c1 == 'y') ? selURY :
		 (c1 == 'X') ? selLRX : selLRY);
	cmd.append(buf);
	break;
      case 'i':
      case 'j':
      case 'k':
	if (!gotMouse) {
	  if (event->type == ButtonPress || event->type == ButtonRelease) {
	    core->cvtWindowToUser(event->xbutton.x, event->xbutton.y,
				  &mousePage, &mouseX, &mouseY);
	  } else if (event->type == KeyPress) {
	    core->cvtWindowToUser(event->xkey.x, event->xkey.y,
				  &mousePage, &mouseX, &mouseY);
	  } else {
	    mousePage = 0;
	    mouseX = mouseY = 0;
	  }
	  gotMouse = true;
	}
	if (c1 == 'i') {
	  cmd.append(std::to_string(mousePage));
	} else {
	  snprintf(buf, sizeof buf, "%g", (c1 == 'j') ? mouseX : mouseY);
	  cmd.append(buf);
	}
	break;
      default:
	cmd.push_back(c1);
	break;
      }
      i += 2;
    } else {
      cmd.push_back(c0);
      ++i;
    }
  }
  cmd.append(" &");
  errcode = system(cmd.c_str());
  if (errcode != 0) {
      error(errInternal, -1, "non-zero error code return by system call");
  }
}

void XPDFViewer::cmdSaveAs(const CmdList& args, XEvent *event) {
  mapSaveAsDialog();
}

void XPDFViewer::cmdScrollDown(const CmdList& args, XEvent *event) {
  core->scrollDown(atoi(args[0].c_str()));
}

void XPDFViewer::cmdScrollDownNextPage(const CmdList& args, XEvent *event) {
  core->scrollDownNextPage(atoi(args[0].c_str()));
}

void XPDFViewer::cmdScrollLeft(const CmdList& args, XEvent *event) {
  core->scrollLeft(atoi(args[0].c_str()));
}

void XPDFViewer::cmdScrollOutlineDown(const CmdList& args, XEvent *event) {
#ifndef DISABLE_OUTLINE
  Widget sb;
  int val, inc, pageInc, m, slider;

  if (outlineScroll == None) {
    return;
  }
  if ((sb = XtNameToWidget(outlineScroll, "VertScrollBar"))) {
    XtVaGetValues(sb, XmNvalue, &val, XmNincrement, &inc,
		  XmNpageIncrement, &pageInc, XmNmaximum, &m,
		  XmNsliderSize, &slider, NULL);
    if ((val += inc * atoi(args[0].c_str())) > m - slider) {
      val = m - slider;
    }
    XmScrollBarSetValues(sb, val, slider, inc, pageInc, True);
  }
#endif
}

void XPDFViewer::cmdScrollOutlineUp(const CmdList& args, XEvent *event) {
#ifndef DISABLE_OUTLINE
  Widget sb;
  int val, inc, pageInc, m, slider;

  if (outlineScroll == None) {
    return;
  }
  if ((sb = XtNameToWidget(outlineScroll, "VertScrollBar"))) {
    XtVaGetValues(sb, XmNvalue, &val, XmNincrement, &inc,
		  XmNpageIncrement, &pageInc, XmNminimum, &m,
		  XmNsliderSize, &slider, NULL);
    if ((val -= inc * atoi(args[0].c_str())) < m) {
      val = m;
    }
    XmScrollBarSetValues(sb, val, slider, inc, pageInc, True);
  }
#endif
}

void XPDFViewer::cmdScrollRight(const CmdList& args, XEvent *event) {
  core->scrollRight(atoi(args[0].c_str()));
}

void XPDFViewer::cmdScrollToBottomEdge(const CmdList& args, XEvent *event) {
  core->scrollToBottomEdge();
}

void XPDFViewer::cmdScrollToBottomRight(const CmdList& args, XEvent *event) {
  core->scrollToBottomRight();
}

void XPDFViewer::cmdScrollToLeftEdge(const CmdList& args, XEvent *event) {
  core->scrollToLeftEdge();
}

void XPDFViewer::cmdScrollToRightEdge(const CmdList& args, XEvent *event) {
  core->scrollToRightEdge();
}

void XPDFViewer::cmdScrollToTopEdge(const CmdList& args, XEvent *event) {
  core->scrollToTopEdge();
}

void XPDFViewer::cmdScrollToTopLeft(const CmdList& args, XEvent *event) {
  core->scrollToTopLeft();
}

void XPDFViewer::cmdScrollUp(const CmdList& args, XEvent *event) {
  core->scrollUp(atoi(args[0].c_str()));
}

void XPDFViewer::cmdScrollUpPrevPage(const CmdList& args, XEvent *event) {
  core->scrollUpPrevPage(atoi(args[0].c_str()));
}

void XPDFViewer::cmdSearch(const CmdList& args, XEvent *event) {
  const char *searchText = args[0].c_str();

  core->find(searchText,
    false,
    false,
    false,
    false,
    false);
}

void XPDFViewer::cmdSetSelection(const CmdList& args, XEvent *event) {
  int pg, ulx, uly, lrx, lry;

  pg = atoi(args[0].c_str());
  core->cvtUserToDev(core->getPageNum(),
		     atof(args[1].c_str()), atof(args[2].c_str()),
		     &ulx, &uly);
  core->cvtUserToDev(core->getPageNum(),
		     atof(args[3].c_str()), atof(args[4].c_str()),
		     &lrx, &lry);
  core->setSelection(pg, ulx, uly, lrx, lry);
}

void XPDFViewer::cmdSinglePageMode(const CmdList& args, XEvent *event) {
  Widget btn;

  if (!core->getContinuousMode()) {
    return;
  }
  core->setContinuousMode(false);

  btn = XtNameToWidget(popupMenu, "continuousMode");
  XtVaSetValues(btn, XmNset, XmUNSET, NULL);
}

void XPDFViewer::cmdStartPan(const CmdList& args, XEvent *event) {
  core->startPan(mouseX(event), mouseY(event));
}

void XPDFViewer::cmdStartSelection(const CmdList& args, XEvent *event) {
  core->startSelection(mouseX(event), mouseY(event));
}

void XPDFViewer::cmdToggleContinuousMode(const CmdList& args, XEvent *event) {
  if (core->getContinuousMode()) {
    cmdSinglePageMode(CmdList(), event);
  } else {
    cmdContinuousMode(CmdList(), event);
  }
}

void XPDFViewer::cmdToggleFullScreenMode(const CmdList& args, XEvent *event) {
  if (core->getFullScreen()) {
    cmdWindowMode(CmdList(), event);
  } else {
    cmdFullScreenMode(CmdList(), event);
  }
}

void XPDFViewer::cmdToggleOutline(const CmdList& args, XEvent *event) {
#ifndef DISABLE_OUTLINE
  Dimension w;

  if (outlineScroll == None) {
    return;
  }
  XtVaGetValues(outlineScroll, XmNwidth, &w, NULL);
  if (w > 1) {
    cmdCloseOutline(CmdList(), event);
  } else {
    cmdOpenOutline(CmdList(), event);
  }
#endif
}

void XPDFViewer::cmdWindowMode(const CmdList& args, XEvent *event) {
  PDFDoc *doc;
  XPDFViewer *viewer;
  int pg;
  Widget btn;

  if (!core->getFullScreen()) {
    return;
  }
  pg = core->getPageNum();
  XtPopdown(win);
  doc = core->takeDoc(false);
  viewer = app->reopen(this, doc, pg, false);

  btn = XtNameToWidget(viewer->popupMenu, "fullScreen");
  XtVaSetValues(btn, XmNset, XmUNSET, NULL);
}

void XPDFViewer::cmdZoomFitPage(const CmdList& args, XEvent *event) {
  if (core->getZoom() != zoomPage) {
    setZoomIdx(zoomPageIdx);
    displayPage(core->getPageNum(), zoomPage,
		core->getRotate(), true, false);
  }
}

void XPDFViewer::cmdZoomFitWidth(const CmdList& args, XEvent *event) {
  if (core->getZoom() != zoomWidth) {
    setZoomIdx(zoomWidthIdx);
    displayPage(core->getPageNum(), zoomWidth,
		core->getRotate(), true, false);
  }
}

void XPDFViewer::cmdZoomFitHeight(const CmdList& args, XEvent *event) {
  if (core->getZoom() != zoomHeight) {
    setZoomIdx(zoomHeightIdx);
    displayPage(core->getPageNum(), zoomHeight,
		core->getRotate(), true, false);
  }
}

void XPDFViewer::cmdZoomIn(const CmdList& args, XEvent *event) {
  int z;

  z = getZoomIdx();
  if (z <= minZoomIdx && z > maxZoomIdx) {
    --z;
    setZoomIdx(z);
    displayPage(core->getPageNum(), zoomMenuInfo[z].zoom,
		core->getRotate(), true, false);
  }
}

void XPDFViewer::cmdZoomOut(const CmdList& args, XEvent *event) {
  int z;

  z = getZoomIdx();
  if (z < minZoomIdx && z >= maxZoomIdx) {
    ++z;
    setZoomIdx(z);
    displayPage(core->getPageNum(), zoomMenuInfo[z].zoom,
		core->getRotate(), true, false);
  }
}

void XPDFViewer::cmdZoomPercent(const CmdList& args, XEvent *event) {
  double z;

  z = atof(args[0].c_str());
  setZoomVal(z);
  displayPage(core->getPageNum(), z, core->getRotate(), true, false);
}

void XPDFViewer::cmdZoomToSelection(const CmdList& args, XEvent *event) {
  int pg;
  double ulx, uly, lrx, lry;

  if (core->getSelection(&pg, &ulx, &uly, &lrx, &lry)) {
    core->zoomToRect(pg, ulx, uly, lrx, lry);
  }
}

//------------------------------------------------------------------------
// GUI code: main window
//------------------------------------------------------------------------

void XPDFViewer::initWindow(bool fullScreen) {
  Colormap colormap;
  XColor xcol;
  Atom state, val;
  Arg args[20];
  int n;
  const char *title;

  display = XtDisplay(app->getAppShell());
  screenNum = XScreenNumberOfScreen(XtScreen(app->getAppShell()));

  toolBar = None;
#ifndef DISABLE_OUTLINE
  outlineScroll = None;
#endif

  // private colormap
  if (app->getInstallCmap()) {
    XtVaGetValues(app->getAppShell(), XmNcolormap, &colormap, NULL);
    // ensure that BlackPixel and WhitePixel are reserved in the
    // new colormap
    xcol.red = xcol.green = xcol.blue = 0;
    XAllocColor(display, colormap, &xcol);
    xcol.red = xcol.green = xcol.blue = 65535;
    XAllocColor(display, colormap, &xcol);
    colormap = XCopyColormapAndFree(display, colormap);
  }

  // top-level window
  n = 0;
  title = app->getTitle() != "" ? app->getTitle().c_str()
				: xpdfAppName;
  XtSetArg(args[n], XmNtitle, title); ++n;
  XtSetArg(args[n], XmNiconName, title); ++n;
  XtSetArg(args[n], XmNminWidth, 100); ++n;
  XtSetArg(args[n], XmNminHeight, 100); ++n;
  XtSetArg(args[n], XmNbaseWidth, 0); ++n;
  XtSetArg(args[n], XmNbaseHeight, 0); ++n;
  XtSetArg(args[n], XmNdeleteResponse, XmDO_NOTHING); ++n;
  win = XtCreatePopupShell("win", topLevelShellWidgetClass,
			   app->getAppShell(), args, n);
  if (app->getInstallCmap()) {
    XtVaSetValues(win, XmNcolormap, colormap, NULL);
  }
  XmAddWMProtocolCallback(win, XInternAtom(display, "WM_DELETE_WINDOW", False),
			  &closeMsgCbk, this);

  // create the full-screen window
  if (fullScreen) {
    initCore(win, true);

  // create the normal (non-full-screen) window
  } else {
    if (app->getGeometry() != "") {
      n = 0;
      XtSetArg(args[n], XmNgeometry, app->getGeometry().c_str()); ++n;
      XtSetValues(win, args, n);
    }

    n = 0;
    form = XmCreateForm(win, "form", args, n);
    XtManageChild(form);

#ifdef DISABLE_OUTLINE
    initToolbar(form);
    n = 0;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); ++n;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); ++n;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); ++n;
    XtSetValues(toolBar, args, n);

    initCore(form, false);
    n = 0;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); ++n;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); ++n;
    XtSetArg(args[n], XmNbottomWidget, toolBar); ++n;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); ++n;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); ++n;
    XtSetValues(core->getWidget(), args, n);
#else
    initToolbar(form);
    n = 0;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); ++n;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); ++n;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); ++n;
    XtSetValues(toolBar, args, n);

    initPanedWin(form);
    n = 0;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); ++n;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); ++n;
    XtSetArg(args[n], XmNbottomWidget, toolBar); ++n;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); ++n;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); ++n;
    XtSetValues(panedWin, args, n);

    initCore(panedWin, fullScreen);
    n = 0;
    XtSetArg(args[n], XmNpositionIndex, 1); ++n;
    XtSetArg(args[n], XmNallowResize, True); ++n;
    XtSetArg(args[n], XmNpaneMinimum, 1); ++n;
    XtSetArg(args[n], XmNpaneMaximum, 10000); ++n;
    XtSetValues(core->getWidget(), args, n);
#endif
  }

  // set the zoom menu to match the initial zoom setting
  setZoomVal(core->getZoom());

  // set traversal order
  XtVaSetValues(core->getDrawAreaWidget(),
		XmNnavigationType, XmTAB_GROUP, NULL);
  if (toolBar != None) {
    XtVaSetValues(backBtn, XmNnavigationType, XmTAB_GROUP, NULL);
    XtVaSetValues(prevTenPageBtn, XmNnavigationType, XmTAB_GROUP, NULL);
    XtVaSetValues(prevPageBtn, XmNnavigationType, XmTAB_GROUP, NULL);
    XtVaSetValues(nextPageBtn, XmNnavigationType, XmTAB_GROUP, NULL);
    XtVaSetValues(nextTenPageBtn, XmNnavigationType, XmTAB_GROUP, NULL);
    XtVaSetValues(forwardBtn, XmNnavigationType, XmTAB_GROUP, NULL);
    XtVaSetValues(pageNumText, XmNnavigationType, XmTAB_GROUP, NULL);
    XtVaSetValues(zoomWidget, XmNnavigationType, XmTAB_GROUP, NULL);
    XtVaSetValues(findBtn, XmNnavigationType, XmTAB_GROUP, NULL);
    XtVaSetValues(printBtn, XmNnavigationType, XmTAB_GROUP, NULL);
    XtVaSetValues(aboutBtn, XmNnavigationType, XmTAB_GROUP, NULL);
    XtVaSetValues(quitBtn, XmNnavigationType, XmTAB_GROUP, NULL);
  }

  initPopupMenu();

  if (fullScreen) {
    // Set both the old-style Motif decorations hint and the new-style
    // _NET_WM_STATE property.  This is redundant, but might be useful
    // for older window managers.  We also set the geometry to +0+0 to
    // avoid interactive placement.  (Note: we need to realize the
    // shell, so it has a Window on which to set the _NET_WM_STATE
    // property, but we don't want to map it until later, so we set
    // mappedWhenManaged to false.)
    n = 0;
    XtSetArg(args[n], XmNmappedWhenManaged, False); ++n;
    XtSetArg(args[n], XmNmwmDecorations, 0); ++n;
    XtSetArg(args[n], XmNgeometry, "+0+0"); ++n;
    XtSetValues(win, args, n);
    XtRealizeWidget(win);
    state = XInternAtom(display, "_NET_WM_STATE", False);
    val = XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", False);
    XChangeProperty(display, XtWindow(win), state, XA_ATOM, 32,
		    PropModeReplace, (unsigned char *)&val, 1);
  }
}

void XPDFViewer::initToolbar(Widget parent) {
  Widget label, lastBtn;
#ifndef USE_COMBO_BOX
  Widget btn;
#endif
  Arg args[20];
  int n;
  XmString s, emptyString;
  int i;

  // toolbar
  n = 0;
  toolBar = XmCreateForm(parent, "toolBar", args, n);
  XtManageChild(toolBar);

  // create an empty string -- this is used for buttons that will get
  // pixmaps later
  emptyString = XmStringCreateLocalized("");

  // page movement buttons
  n = 0;
  XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNmarginWidth, 6); ++n;
  XtSetArg(args[n], XmNsensitive, False); ++n;
  XtSetArg(args[n], XmNlabelString, emptyString); ++n;
  backBtn = XmCreatePushButton(toolBar, "back", args, n);
  addToolTip(backBtn, "Back");
  XtManageChild(backBtn);
  XtAddCallback(backBtn, XmNactivateCallback,
		&backCbk, (XtPointer)this);
  n = 0;
  XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); ++n;
  XtSetArg(args[n], XmNleftWidget, backBtn); ++n;
  XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNmarginWidth, 6); ++n;
  XtSetArg(args[n], XmNsensitive, False); ++n;
  XtSetArg(args[n], XmNlabelString, emptyString); ++n;
  prevTenPageBtn = XmCreatePushButton(toolBar, "prevTenPage", args, n);
  addToolTip(prevTenPageBtn, "-10 pages");
  XtManageChild(prevTenPageBtn);
  XtAddCallback(prevTenPageBtn, XmNactivateCallback,
		&prevTenPageCbk, (XtPointer)this);
  n = 0;
  XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); ++n;
  XtSetArg(args[n], XmNleftWidget, prevTenPageBtn); ++n;
  XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNmarginWidth, 6); ++n;
  XtSetArg(args[n], XmNsensitive, False); ++n;
  XtSetArg(args[n], XmNlabelString, emptyString); ++n;
  prevPageBtn = XmCreatePushButton(toolBar, "prevPage", args, n);
  addToolTip(prevPageBtn, "Previous page");
  XtManageChild(prevPageBtn);
  XtAddCallback(prevPageBtn, XmNactivateCallback,
		&prevPageCbk, (XtPointer)this);
  n = 0;
  XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); ++n;
  XtSetArg(args[n], XmNleftWidget, prevPageBtn); ++n;
  XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNmarginWidth, 6); ++n;
  XtSetArg(args[n], XmNsensitive, False); ++n;
  XtSetArg(args[n], XmNlabelString, emptyString); ++n;
  nextPageBtn = XmCreatePushButton(toolBar, "nextPage", args, n);
  addToolTip(nextPageBtn, "Next page");
  XtManageChild(nextPageBtn);
  XtAddCallback(nextPageBtn, XmNactivateCallback,
		&nextPageCbk, (XtPointer)this);
  n = 0;
  XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); ++n;
  XtSetArg(args[n], XmNleftWidget, nextPageBtn); ++n;
  XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNmarginWidth, 6); ++n;
  XtSetArg(args[n], XmNsensitive, False); ++n;
  XtSetArg(args[n], XmNlabelString, emptyString); ++n;
  nextTenPageBtn = XmCreatePushButton(toolBar, "nextTenPage", args, n);
  addToolTip(nextTenPageBtn, "+10 pages");
  XtManageChild(nextTenPageBtn);
  XtAddCallback(nextTenPageBtn, XmNactivateCallback,
		&nextTenPageCbk, (XtPointer)this);
  n = 0;
  XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); ++n;
  XtSetArg(args[n], XmNleftWidget, nextTenPageBtn); ++n;
  XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNmarginWidth, 6); ++n;
  XtSetArg(args[n], XmNsensitive, False); ++n;
  XtSetArg(args[n], XmNlabelString, emptyString); ++n;
  forwardBtn = XmCreatePushButton(toolBar, "forward", args, n);
  addToolTip(forwardBtn, "Forward");
  XtManageChild(forwardBtn);
  XtAddCallback(forwardBtn, XmNactivateCallback,
		&forwardCbk, (XtPointer)this);

  // page number display
  n = 0;
  XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); ++n;
  XtSetArg(args[n], XmNleftWidget, forwardBtn); ++n;
  XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); ++n;
  s = XmStringCreateLocalized("Page ");
  XtSetArg(args[n], XmNlabelString, s); ++n;
  label = XmCreateLabel(toolBar, "pageLabel", args, n);
  XmStringFree(s);
  XtManageChild(label);
  n = 0;
  XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); ++n;
  XtSetArg(args[n], XmNleftWidget, label); ++n;
  XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNmarginWidth, 3); ++n;
  XtSetArg(args[n], XmNmarginHeight, 3); ++n;
  XtSetArg(args[n], XmNcolumns, 5); ++n;
  pageNumText = XmCreateTextField(toolBar, "pageNum", args, n);
  XtManageChild(pageNumText);
  XtAddCallback(pageNumText, XmNactivateCallback,
		&pageNumCbk, (XtPointer)this);
  n = 0;
  XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); ++n;
  XtSetArg(args[n], XmNleftWidget, pageNumText); ++n;
  XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); ++n;
  s = XmStringCreateLocalized(" of 00000");
  XtSetArg(args[n], XmNlabelString, s); ++n;
  XtSetArg(args[n], XmNalignment, XmALIGNMENT_BEGINNING); ++n;
  XtSetArg(args[n], XmNrecomputeSize, False); ++n;
  pageCountLabel = XmCreateLabel(toolBar, "pageCountLabel", args, n);
  XmStringFree(s);
  XtManageChild(pageCountLabel);
  s = XmStringCreateLocalized(" of 0");
  XtVaSetValues(pageCountLabel, XmNlabelString, s, NULL);
  XmStringFree(s);

  // zoom menu
#if USE_COMBO_BOX
  XmString st[nZoomMenuItems];
  n = 0;
  XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); ++n;
  XtSetArg(args[n], XmNleftWidget, pageCountLabel); ++n;
  XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNmarginWidth, 0); ++n;
  XtSetArg(args[n], XmNmarginHeight, 0); ++n;
  XtSetArg(args[n], XmNcomboBoxType, XmDROP_DOWN_COMBO_BOX); ++n;
  XtSetArg(args[n], XmNpositionMode, XmONE_BASED); ++n;
  XtSetArg(args[n], XmNcolumns, 7); ++n;
  for (i = 0; i < nZoomMenuItems; ++i) {
    st[i] = XmStringCreateLocalized((char *)zoomMenuInfo[i].label);
  }
  XtSetArg(args[n], XmNitems, st); ++n;
  XtSetArg(args[n], XmNitemCount, nZoomMenuItems); ++n;
  zoomComboBox = XmCreateComboBox(toolBar, "zoomComboBox", args, n);
  for (i = 0; i < nZoomMenuItems; ++i) {
    XmStringFree(st[i]);
  }
  addToolTip(zoomComboBox, "Zoom");
  XtAddCallback(zoomComboBox, XmNselectionCallback,
		&zoomComboBoxCbk, (XtPointer)this);
  XtManageChild(zoomComboBox);
  zoomWidget = zoomComboBox;
#else
  Widget menuPane;
  char buf[16];
  n = 0;
  menuPane = XmCreatePulldownMenu(toolBar, "zoomMenuPane", args, n);
  for (i = 0; i < nZoomMenuItems; ++i) {
    n = 0;
    s = XmStringCreateLocalized((char *)zoomMenuInfo[i].label);
    XtSetArg(args[n], XmNlabelString, s); ++n;
    XtSetArg(args[n], XmNuserData, i); ++n;
    snprintf(buf, sizeof buf, "zoom%d", i);
    btn = XmCreatePushButton(menuPane, buf, args, n);
    XmStringFree(s);
    XtManageChild(btn);
    XtAddCallback(btn, XmNactivateCallback,
		  &zoomMenuCbk, (XtPointer)this);
    zoomMenuBtns[i] = btn;
  }
  n = 0;
  XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); ++n;
  XtSetArg(args[n], XmNleftWidget, pageCountLabel); ++n;
  XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNmarginWidth, 0); ++n;
  XtSetArg(args[n], XmNmarginHeight, 0); ++n;
  XtSetArg(args[n], XmNsubMenuId, menuPane); ++n;
  zoomMenu = XmCreateOptionMenu(toolBar, "zoomMenu", args, n);
  addToolTip(zoomMenu, "Zoom");
  XtManageChild(zoomMenu);
  zoomWidget = zoomMenu;
#endif

  // find/print/about buttons
  n = 0;
  XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); ++n;
  XtSetArg(args[n], XmNleftWidget, zoomWidget); ++n;
  XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNmarginWidth, 6); ++n;
  XtSetArg(args[n], XmNlabelString, emptyString); ++n;
  findBtn = XmCreatePushButton(toolBar, "find", args, n);
  addToolTip(findBtn, "Find");
  XtManageChild(findBtn);
  XtAddCallback(findBtn, XmNactivateCallback,
		&findCbk, (XtPointer)this);
  n = 0;
  XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); ++n;
  XtSetArg(args[n], XmNleftWidget, findBtn); ++n;
  XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNmarginWidth, 6); ++n;
  XtSetArg(args[n], XmNlabelString, emptyString); ++n;
  printBtn = XmCreatePushButton(toolBar, "print", args, n);
  addToolTip(printBtn, "Print");
  XtManageChild(printBtn);
  XtAddCallback(printBtn, XmNactivateCallback,
		&printCbk, (XtPointer)this);
  n = 0;
  XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); ++n;
  XtSetArg(args[n], XmNleftWidget, printBtn); ++n;
  XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNmarginWidth, 6); ++n;
  XtSetArg(args[n], XmNlabelString, emptyString); ++n;
  aboutBtn = XmCreatePushButton(toolBar, "about", args, n);
  addToolTip(aboutBtn, "About / help");
  XtManageChild(aboutBtn);
  XtAddCallback(aboutBtn, XmNactivateCallback,
		&aboutCbk, (XtPointer)this);
  lastBtn = aboutBtn;

  // quit button
  n = 0;
  XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNmarginWidth, 6); ++n;
  s = XmStringCreateLocalized("Quit");
  XtSetArg(args[n], XmNlabelString, s); ++n;
  quitBtn = XmCreatePushButton(toolBar, "quit", args, n);
  XmStringFree(s);
  XtManageChild(quitBtn);
  XtAddCallback(quitBtn, XmNactivateCallback,
		&quitCbk, (XtPointer)this);

  // link label
  n = 0;
  XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); ++n;
  XtSetArg(args[n], XmNleftWidget, lastBtn); ++n;
  XtSetArg(args[n], XmNrightAttachment, XmATTACH_WIDGET); ++n;
  XtSetArg(args[n], XmNrightWidget, quitBtn); ++n;
  XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); ++n;
  s = XmStringCreateLocalized("");
  XtSetArg(args[n], XmNlabelString, s); ++n;
  XtSetArg(args[n], XmNrecomputeSize, True); ++n;
  XtSetArg(args[n], XmNalignment, XmALIGNMENT_BEGINNING); ++n;
  linkLabel = XmCreateLabel(toolBar, "linkLabel", args, n);
  XmStringFree(s);
  XtManageChild(linkLabel);

  XmStringFree(emptyString);
}

#ifndef DISABLE_OUTLINE
void XPDFViewer::initPanedWin(Widget parent) {
  Widget clipWin;
  Arg args[20];
  int n;

  // paned window
  n = 0;
  XtSetArg(args[n], XmNorientation, XmHORIZONTAL); ++n;
#if defined(__sgi) && (XmVERSION <= 1)
  panedWin = SgCreateHorzPanedWindow(parent, "panedWin", args, n);
#else
  panedWin = XmCreatePanedWindow(parent, "panedWin", args, n);
#endif
  XtManageChild(panedWin);

  // scrolled window for outline container
  n = 0;
  XtSetArg(args[n], XmNpositionIndex, 0); ++n;
  XtSetArg(args[n], XmNallowResize, True); ++n;
  XtSetArg(args[n], XmNpaneMinimum, 1); ++n;
  XtSetArg(args[n], XmNpaneMaximum, 10000); ++n;
#if !(defined(__sgi) && (XmVERSION <= 1))
  XtSetArg(args[n], XmNwidth, 1); ++n;
#endif
  XtSetArg(args[n], XmNscrollingPolicy, XmAUTOMATIC); ++n;
  outlineScroll = XmCreateScrolledWindow(panedWin, "outlineScroll", args, n);
  XtManageChild(outlineScroll);
  XtVaGetValues(outlineScroll, XmNclipWindow, &clipWin, NULL);
  XtVaSetValues(clipWin, XmNbackground, app->getPaperPixel(), NULL);

  // outline tree
  n = 0;
  XtSetArg(args[n], XmNbackground, app->getPaperPixel()); ++n;
  outlineTree = XPDFCreateTree(outlineScroll, "outlineTree", args, n);
  XtManageChild(outlineTree);
  XtAddCallback(outlineTree, XPDFNselectionCallback, &outlineSelectCbk,
		(XtPointer)this);
}
#endif

void XPDFViewer::initCore(Widget parent, bool fullScreen) {
  core = new XPDFCore(win, parent,
		      app->getPaperRGB(), app->getPaperPixel(),
		      app->getMattePixel(fullScreen),
		      fullScreen, app->getReverseVideo(),
		      app->getInstallCmap(), app->getRGBCubeSize());
  core->setUpdateCbk(&updateCbk, this);
  core->setActionCbk(&actionCbk, this);
  core->setKeyPressCbk(&keyPressCbk, this);
  core->setMouseCbk(&mouseCbk, this);
}

void XPDFViewer::initPopupMenu() {
  Widget btn;
  Arg args[20];
  int n;
  XmString s, s2;

  n = 0;
#if XmVersion < 1002
  // older versions of Motif need this, newer ones choke on it,
  // sometimes not displaying the menu at all, maybe depending on the
  // state of the NumLock key (taken from DDD)
  XtSetArg(args[n], XmNmenuPost, "<Btn3Down>"); ++n;
#endif
  popupMenu = XmCreatePopupMenu(core->getDrawAreaWidget(), "popupMenu",
				args, n);
  n = 0;
  s = XmStringCreateLocalized("Open...");
  XtSetArg(args[n], XmNlabelString, s); ++n;
  s2 = XmStringCreateLocalized("O");
  XtSetArg(args[n], XmNacceleratorText, s2); ++n;
  btn = XmCreatePushButton(popupMenu, "open", args, n);
  XmStringFree(s);
  XmStringFree(s2);
  XtManageChild(btn);
  XtAddCallback(btn, XmNactivateCallback,
		&openCbk, (XtPointer)this);
  n = 0;
  s = XmStringCreateLocalized("Open in new window...");
  XtSetArg(args[n], XmNlabelString, s); ++n;
  btn = XmCreatePushButton(popupMenu, "openInNewWindow", args, n);
  XmStringFree(s);
  XtManageChild(btn);
  XtAddCallback(btn, XmNactivateCallback,
		&openInNewWindowCbk, (XtPointer)this);
  n = 0;
  s = XmStringCreateLocalized("Reload");
  XtSetArg(args[n], XmNlabelString, s); ++n;
  s2 = XmStringCreateLocalized("R");
  XtSetArg(args[n], XmNacceleratorText, s2); ++n;
  btn = XmCreatePushButton(popupMenu, "reload", args, n);
  XmStringFree(s);
  XmStringFree(s2);
  XtManageChild(btn);
  XtAddCallback(btn, XmNactivateCallback,
		&reloadCbk, (XtPointer)this);
  n = 0;
  s = XmStringCreateLocalized("Save as...");
  XtSetArg(args[n], XmNlabelString, s); ++n;
  s2 = XmStringCreateLocalized("S");
  XtSetArg(args[n], XmNacceleratorText, s2); ++n;
  btn = XmCreatePushButton(popupMenu, "saveAs", args, n);
  XmStringFree(s);
  XtManageChild(btn);
  XtAddCallback(btn, XmNactivateCallback,
		&saveAsCbk, (XtPointer)this);
  n = 0;
  btn = XmCreateSeparator(popupMenu, "sep1", args, n);
  XtManageChild(btn);
  n = 0;
  s = XmStringCreateLocalized("Continuous view");
  XtSetArg(args[n], XmNlabelString, s); ++n;
  XtSetArg(args[n], XmNindicatorType, XmN_OF_MANY); ++n;
  XtSetArg(args[n], XmNvisibleWhenOff, True); ++n;
  XtSetArg(args[n], XmNset, core->getContinuousMode() ? XmSET : XmUNSET); ++n;
  btn = XmCreateToggleButton(popupMenu, "continuousMode", args, n);
  XmStringFree(s);
  XtManageChild(btn);
  XtAddCallback(btn, XmNvalueChangedCallback,
		&continuousModeToggleCbk, (XtPointer)this);
  n = 0;
  s = XmStringCreateLocalized("Full screen");
  XtSetArg(args[n], XmNlabelString, s); ++n;
  XtSetArg(args[n], XmNindicatorType, XmN_OF_MANY); ++n;
  XtSetArg(args[n], XmNvisibleWhenOff, True); ++n;
  XtSetArg(args[n], XmNset, core->getFullScreen() ? XmSET : XmUNSET); ++n;
  btn = XmCreateToggleButton(popupMenu, "fullScreen", args, n);
  XmStringFree(s);
  XtManageChild(btn);
  XtAddCallback(btn, XmNvalueChangedCallback,
		&fullScreenToggleCbk, (XtPointer)this);
  n = 0;
  s = XmStringCreateLocalized("Rotate counterclockwise");
  XtSetArg(args[n], XmNlabelString, s); ++n;
  s2 = XmStringCreateLocalized("[");
  XtSetArg(args[n], XmNacceleratorText, s2); ++n;
  btn = XmCreatePushButton(popupMenu, "rotateCCW", args, n);
  XmStringFree(s);
  XtManageChild(btn);
  XtAddCallback(btn, XmNactivateCallback,
		&rotateCCWCbk, (XtPointer)this);
  n = 0;
  s = XmStringCreateLocalized("Rotate clockwise");
  XtSetArg(args[n], XmNlabelString, s); ++n;
  s2 = XmStringCreateLocalized("]");
  XtSetArg(args[n], XmNacceleratorText, s2); ++n;
  btn = XmCreatePushButton(popupMenu, "rotateCW", args, n);
  XmStringFree(s);
  XtManageChild(btn);
  XtAddCallback(btn, XmNactivateCallback,
		&rotateCWCbk, (XtPointer)this);
  n = 0;
  s = XmStringCreateLocalized("Zoom to selection");
  XtSetArg(args[n], XmNlabelString, s); ++n;
  btn = XmCreatePushButton(popupMenu, "zoomToSelection", args, n);
  XmStringFree(s);
  XtManageChild(btn);
  XtAddCallback(btn, XmNactivateCallback,
		&zoomToSelectionCbk, (XtPointer)this);
  n = 0;
  btn = XmCreateSeparator(popupMenu, "sep2", args, n);
  XtManageChild(btn);
  n = 0;
  s = XmStringCreateLocalized("Close");
  XtSetArg(args[n], XmNlabelString, s); ++n;
  s2 = XmStringCreateLocalized("Ctrl+W");
  XtSetArg(args[n], XmNacceleratorText, s2); ++n;
  btn = XmCreatePushButton(popupMenu, "close", args, n);
  XmStringFree(s);
  XmStringFree(s2);
  XtManageChild(btn);
  XtAddCallback(btn, XmNactivateCallback,
		&closeCbk, (XtPointer)this);
  n = 0;
  s = XmStringCreateLocalized("Quit");
  XtSetArg(args[n], XmNlabelString, s); ++n;
  s2 = XmStringCreateLocalized("Q");
  XtSetArg(args[n], XmNacceleratorText, s2); ++n;
  btn = XmCreatePushButton(popupMenu, "quit", args, n);
  XmStringFree(s);
  XmStringFree(s2);
  XtManageChild(btn);
  XtAddCallback(btn, XmNactivateCallback,
		&quitCbk, (XtPointer)this);

  // this is magic (taken from DDD) - weird things happen if this
  // call isn't made
  XtUngrabButton(core->getDrawAreaWidget(), AnyButton, AnyModifier);
}

void XPDFViewer::addToolTip(Widget widget, char *text) {
#ifdef XmNtoolTipString
  XmString s;
  Cardinal n, i;
  WidgetList children;

  if (XtIsComposite(widget)) {
    XtVaGetValues(widget, XmNnumChildren, &n, XmNchildren, &children, NULL);
    for (i = 0; i < n; ++i) {
      addToolTip(children[i], text);
    }
  } else {
    s = XmStringCreateLocalized(text);
    XtVaSetValues(widget, XmNtoolTipString, s, NULL);
    XmStringFree(s);
  }
#endif
}

void XPDFViewer::mapWindow() {
#ifdef HAVE_X11_XPM_H
  Pixmap iconPixmap;
#endif
  int depth;
  Pixel fg, bg, arm;

  // show the window
  XtPopup(win, XtGrabNone);
  core->takeFocus();

  // create the icon
#ifdef HAVE_X11_XPM_H
  if (XpmCreatePixmapFromData(display, XtWindow(win), xpdfIcon,
			      &iconPixmap, NULL, NULL) == XpmSuccess) {
    XtVaSetValues(win, XmNiconPixmap, iconPixmap, NULL);
  }
#endif

  // set button bitmaps (must be done after the window is mapped)
  if (toolBar != None) {
    XtVaGetValues(backBtn, XmNdepth, &depth,
		  XmNforeground, &fg, XmNbackground, &bg,
		  XmNarmColor, &arm, NULL);
    XtVaSetValues(backBtn, XmNlabelType, XmPIXMAP,
		  XmNlabelPixmap,
		  XCreatePixmapFromBitmapData(display, XtWindow(toolBar),
					      (char *)backArrow_bits,
					      backArrow_width,
					      backArrow_height,
					      fg, bg, depth),
		  XmNarmPixmap,
		  XCreatePixmapFromBitmapData(display, XtWindow(toolBar),
					      (char *)backArrow_bits,
					      backArrow_width,
					      backArrow_height,
					      fg, arm, depth),
		  XmNlabelInsensitivePixmap,
		  XCreatePixmapFromBitmapData(display, XtWindow(toolBar),
					      (char *)backArrowDis_bits,
					      backArrowDis_width,
					      backArrowDis_height,
					      fg, bg, depth),
		  NULL);
    XtVaSetValues(prevTenPageBtn, XmNlabelType, XmPIXMAP,
		  XmNlabelPixmap,
		  XCreatePixmapFromBitmapData(display, XtWindow(toolBar),
					      (char *)dblLeftArrow_bits,
					      dblLeftArrow_width,
					      dblLeftArrow_height,
					      fg, bg, depth),
		  XmNarmPixmap,
		  XCreatePixmapFromBitmapData(display, XtWindow(toolBar),
					      (char *)dblLeftArrow_bits,
					      dblLeftArrow_width,
					      dblLeftArrow_height,
					      fg, arm, depth),
		  XmNlabelInsensitivePixmap,
		  XCreatePixmapFromBitmapData(display, XtWindow(toolBar),
					      (char *)dblLeftArrowDis_bits,
					      dblLeftArrowDis_width,
					      dblLeftArrowDis_height,
					      fg, bg, depth),
		  NULL);
    XtVaSetValues(prevPageBtn, XmNlabelType, XmPIXMAP,
		  XmNlabelPixmap,
		  XCreatePixmapFromBitmapData(display, XtWindow(toolBar),
					      (char *)leftArrow_bits,
					      leftArrow_width,
					      leftArrow_height,
					      fg, bg, depth),
		  XmNarmPixmap,
		  XCreatePixmapFromBitmapData(display, XtWindow(toolBar),
					      (char *)leftArrow_bits,
					      leftArrow_width,
					      leftArrow_height,
					      fg, arm, depth),
		  XmNlabelInsensitivePixmap,
		  XCreatePixmapFromBitmapData(display, XtWindow(toolBar),
					      (char *)leftArrowDis_bits,
					      leftArrowDis_width,
					      leftArrowDis_height,
					      fg, bg, depth),
		  NULL);
    XtVaSetValues(nextPageBtn, XmNlabelType, XmPIXMAP,
		  XmNlabelPixmap,
		  XCreatePixmapFromBitmapData(display, XtWindow(toolBar),
					      (char *)rightArrow_bits,
					      rightArrow_width,
					      rightArrow_height,
					      fg, bg, depth),
		  XmNarmPixmap,
		  XCreatePixmapFromBitmapData(display, XtWindow(toolBar),
					      (char *)rightArrow_bits,
					      rightArrow_width,
					      rightArrow_height,
					      fg, arm, depth),
		  XmNlabelInsensitivePixmap,
		  XCreatePixmapFromBitmapData(display, XtWindow(toolBar),
					      (char *)rightArrowDis_bits,
					      rightArrowDis_width,
					      rightArrowDis_height,
					      fg, bg, depth),
		  NULL);
    XtVaSetValues(nextTenPageBtn, XmNlabelType, XmPIXMAP,
		  XmNlabelPixmap,
		  XCreatePixmapFromBitmapData(display, XtWindow(toolBar),
					      (char *)dblRightArrow_bits,
					      dblRightArrow_width,
					      dblRightArrow_height,
					      fg, bg, depth),
		  XmNarmPixmap,
		  XCreatePixmapFromBitmapData(display, XtWindow(toolBar),
					      (char *)dblRightArrow_bits,
					      dblRightArrow_width,
					      dblRightArrow_height,
					      fg, arm, depth),
		  XmNlabelInsensitivePixmap,
		  XCreatePixmapFromBitmapData(display, XtWindow(toolBar),
					      (char *)dblRightArrowDis_bits,
					      dblRightArrowDis_width,
					      dblRightArrowDis_height,
					      fg, bg, depth),
		  NULL);
    XtVaSetValues(forwardBtn, XmNlabelType, XmPIXMAP,
		  XmNlabelPixmap,
		  XCreatePixmapFromBitmapData(display, XtWindow(toolBar),
					      (char *)forwardArrow_bits,
					      forwardArrow_width,
					      forwardArrow_height,
					      fg, bg, depth),
		  XmNarmPixmap,
		  XCreatePixmapFromBitmapData(display, XtWindow(toolBar),
					      (char *)forwardArrow_bits,
					      forwardArrow_width,
					      forwardArrow_height,
					      fg, arm, depth),
		  XmNlabelInsensitivePixmap,
		  XCreatePixmapFromBitmapData(display, XtWindow(toolBar),
					      (char *)forwardArrowDis_bits,
					      forwardArrowDis_width,
					      forwardArrowDis_height,
					      fg, bg, depth),
		  NULL);
    XtVaSetValues(findBtn, XmNlabelType, XmPIXMAP,
		  XmNlabelPixmap,
		  XCreatePixmapFromBitmapData(display, XtWindow(toolBar),
					      (char *)find_bits,
					      find_width,
					      find_height,
					      fg, bg, depth),
		  XmNarmPixmap,
		  XCreatePixmapFromBitmapData(display, XtWindow(toolBar),
					      (char *)find_bits,
					      find_width,
					      find_height,
					      fg, arm, depth),
		  XmNlabelInsensitivePixmap,
		  XCreatePixmapFromBitmapData(display, XtWindow(toolBar),
					      (char *)findDis_bits,
					      findDis_width,
					      findDis_height,
					      fg, bg, depth),
		  NULL);
    XtVaSetValues(printBtn, XmNlabelType, XmPIXMAP,
		  XmNlabelPixmap,
		  XCreatePixmapFromBitmapData(display, XtWindow(toolBar),
					      (char *)print_bits,
					      print_width,
					      print_height,
					      fg, bg, depth),
		  XmNarmPixmap,
		  XCreatePixmapFromBitmapData(display, XtWindow(toolBar),
					      (char *)print_bits,
					      print_width,
					      print_height,
					      fg, arm, depth),
		  XmNlabelInsensitivePixmap,
		  XCreatePixmapFromBitmapData(display, XtWindow(toolBar),
					      (char *)printDis_bits,
					      printDis_width,
					      printDis_height,
					      fg, bg, depth),
		  NULL);
    XtVaSetValues(aboutBtn, XmNlabelType, XmPIXMAP,
		  XmNlabelPixmap,
		  XCreatePixmapFromBitmapData(display, XtWindow(toolBar),
					      (char *)about_bits,
					      about_width,
					      about_height,
					      fg, bg, depth),
		  XmNarmPixmap,
		  XCreatePixmapFromBitmapData(display, XtWindow(toolBar),
					      (char *)about_bits,
					      about_width,
					      about_height,
					      fg, arm, depth),
		  NULL);
  }
}

void XPDFViewer::closeWindow() {
  XtPopdown(win);
  XtDestroyWidget(win);
}

int XPDFViewer::getZoomIdx() {
  int i;

  for (i = 0; i < nZoomMenuItems; ++i) {
    if (core->getZoom() == zoomMenuInfo[i].zoom) {
      return i;
    }
  }
  return -1;
}

void XPDFViewer::setZoomIdx(int idx) {
  if (toolBar == None) {
    return;
  }
#if USE_COMBO_BOX
  XtVaSetValues(zoomComboBox, XmNselectedPosition, idx + 1, NULL);
#else
  XtVaSetValues(zoomMenu, XmNmenuHistory, zoomMenuBtns[idx], NULL);
#endif
}

void XPDFViewer::setZoomVal(double z) {
  if (toolBar == None) {
    return;
  }

#if USE_COMBO_BOX
  char buf[32];
  XmString s;
  int i;

  for (i = 0; i < nZoomMenuItems; ++i) {
    if (z == zoomMenuInfo[i].zoom) {
      XtVaSetValues(zoomComboBox, XmNselectedPosition, i + 1, NULL);
      return;
    }
  }
  snprintf(buf, sizeof buf, "%d%%", (int)z);
  s = XmStringCreateLocalized(buf);
  XtVaSetValues(zoomComboBox, XmNselectedItem, s, NULL);
  XmStringFree(s);
#else
  int i;

  for (i = 0; i < nZoomMenuItems; ++i) {
    if (z == zoomMenuInfo[i].zoom) {
      XtVaSetValues(zoomMenu, XmNmenuHistory, zoomMenuBtns[i], NULL);
      return;
    }
  }
  for (i = maxZoomIdx; i < minZoomIdx; ++i) {
    if (z > zoomMenuInfo[i].zoom) {
      break;
    }
  }
  XtVaSetValues(zoomMenu, XmNmenuHistory, zoomMenuBtns[i], NULL);
#endif
}

void XPDFViewer::prevPageCbk(Widget widget, XtPointer ptr,
			     XtPointer callData) {
  XPDFViewer *viewer = (XPDFViewer *)ptr;

  viewer->core->gotoPrevPage(1, true, false);
  viewer->core->takeFocus();
}

void XPDFViewer::prevTenPageCbk(Widget widget, XtPointer ptr,
				XtPointer callData) {
  XPDFViewer *viewer = (XPDFViewer *)ptr;

  viewer->core->gotoPrevPage(10, true, false);
  viewer->core->takeFocus();
}

void XPDFViewer::nextPageCbk(Widget widget, XtPointer ptr,
			     XtPointer callData) {
  XPDFViewer *viewer = (XPDFViewer *)ptr;

  viewer->core->gotoNextPage(1, true);
  viewer->core->takeFocus();
}

void XPDFViewer::nextTenPageCbk(Widget widget, XtPointer ptr,
				XtPointer callData) {
  XPDFViewer *viewer = (XPDFViewer *)ptr;

  viewer->core->gotoNextPage(10, true);
  viewer->core->takeFocus();
}

void XPDFViewer::backCbk(Widget widget, XtPointer ptr,
			 XtPointer callData) {
  XPDFViewer *viewer = (XPDFViewer *)ptr;

  viewer->core->goBackward();
  viewer->core->takeFocus();
}

void XPDFViewer::forwardCbk(Widget widget, XtPointer ptr,
			    XtPointer callData) {
  XPDFViewer *viewer = (XPDFViewer *)ptr;

  viewer->core->goForward();
  viewer->core->takeFocus();
}

#if USE_COMBO_BOX

void XPDFViewer::zoomComboBoxCbk(Widget widget, XtPointer ptr,
				 XtPointer callData) {
  XPDFViewer *viewer = (XPDFViewer *)ptr;
  XmComboBoxCallbackStruct *data = (XmComboBoxCallbackStruct *)callData;
  double z;
  char *s;
  XmStringContext context;
  XmStringCharSet charSet;
  XmStringDirection dir;
  Boolean sep;

  z = viewer->core->getZoom();
  if (data->item_position == 0) {
    XmStringInitContext(&context, data->item_or_text);
    if (XmStringGetNextSegment(context, &s, &charSet, &dir, &sep)) {
      z = atof(s);
      if (z <= 1) {
	z = defZoom;
      }
      XtFree(charSet);
      XtFree(s);
    }
    XmStringFreeContext(context);
  } else {
    z = zoomMenuInfo[data->item_position - 1].zoom;
  }
  // only redraw if this was triggered by an event; otherwise
  // the caller is responsible for doing the redraw
  if (z != viewer->core->getZoom() && data->event) {
    viewer->displayPage(viewer->core->getPageNum(), z,
			viewer->core->getRotate(), true, false);
  }
  viewer->core->takeFocus();
}

#else // USE_COMBO_BOX

void XPDFViewer::zoomMenuCbk(Widget widget, XtPointer ptr,
			     XtPointer callData) {
  XPDFViewer *viewer = (XPDFViewer *)ptr;
  XmPushButtonCallbackStruct *data = (XmPushButtonCallbackStruct *)callData;
  XtPointer userData;
  double z;

  XtVaGetValues(widget, XmNuserData, &userData, NULL);
  z = zoomMenuInfo[(long)userData].zoom;
  // only redraw if this was triggered by an event; otherwise
  // the caller is responsible for doing the redraw
  if (z != viewer->core->getZoom() && data->event) {
    viewer->displayPage(viewer->core->getPageNum(), z,
			viewer->core->getRotate(), true, false);
  }
  viewer->core->takeFocus();
}

#endif // USE_COMBO_BOX

void XPDFViewer::findCbk(Widget widget, XtPointer ptr,
			 XtPointer callData) {
  XPDFViewer *viewer = (XPDFViewer *)ptr;

  if (!viewer->core->getDoc()) {
    return;
  }
  viewer->mapFindDialog();
}

void XPDFViewer::printCbk(Widget widget, XtPointer ptr,
			  XtPointer callData) {
  XPDFViewer *viewer = (XPDFViewer *)ptr;

  if (!viewer->core->getDoc()) {
    return;
  }
  XtManageChild(viewer->printDialog);
}

void XPDFViewer::aboutCbk(Widget widget, XtPointer ptr,
			  XtPointer callData) {
  XPDFViewer *viewer = (XPDFViewer *)ptr;

  XtManageChild(viewer->aboutDialog);
}

void XPDFViewer::quitCbk(Widget widget, XtPointer ptr,
			 XtPointer callData) {
  XPDFViewer *viewer = (XPDFViewer *)ptr;

  viewer->app->quit();
}

void XPDFViewer::openCbk(Widget widget, XtPointer ptr,
			 XtPointer callData) {
  XPDFViewer *viewer = (XPDFViewer *)ptr;

  viewer->mapOpenDialog(false);
}

void XPDFViewer::openInNewWindowCbk(Widget widget, XtPointer ptr,
				    XtPointer callData) {
  XPDFViewer *viewer = (XPDFViewer *)ptr;

  viewer->mapOpenDialog(true);
}

void XPDFViewer::reloadCbk(Widget widget, XtPointer ptr,
			 XtPointer callData) {
  XPDFViewer *viewer = (XPDFViewer *)ptr;

  viewer->reloadFile();
}

void XPDFViewer::saveAsCbk(Widget widget, XtPointer ptr,
			   XtPointer callData) {
  XPDFViewer *viewer = (XPDFViewer *)ptr;

  if (!viewer->core->getDoc()) {
    return;
  }
  viewer->mapSaveAsDialog();
}

void XPDFViewer::continuousModeToggleCbk(Widget widget, XtPointer ptr,
					 XtPointer callData) {
  XPDFViewer *viewer = (XPDFViewer *)ptr;
  XmToggleButtonCallbackStruct *data =
      (XmToggleButtonCallbackStruct *)callData;

  viewer->core->setContinuousMode(data->set == XmSET);
}

void XPDFViewer::fullScreenToggleCbk(Widget widget, XtPointer ptr,
				     XtPointer callData) {
  XPDFViewer *viewer = (XPDFViewer *)ptr;
  XmToggleButtonCallbackStruct *data =
      (XmToggleButtonCallbackStruct *)callData;

  if (data->set == XmSET) {
    viewer->cmdFullScreenMode(CmdList(), NULL);
  } else {
    viewer->cmdWindowMode(CmdList(), NULL);
  }
}

void XPDFViewer::rotateCCWCbk(Widget widget, XtPointer ptr,
			      XtPointer callData) {
  XPDFViewer *viewer = (XPDFViewer *)ptr;

  viewer->cmdRotateCCW(CmdList(), NULL);
}

void XPDFViewer::rotateCWCbk(Widget widget, XtPointer ptr,
			     XtPointer callData) {
  XPDFViewer *viewer = (XPDFViewer *)ptr;

  viewer->cmdRotateCW(CmdList(), NULL);
}

void XPDFViewer::zoomToSelectionCbk(Widget widget, XtPointer ptr,
				    XtPointer callData) {
  XPDFViewer *viewer = (XPDFViewer *)ptr;
  int pg;
  double ulx, uly, lrx, lry;

  if (viewer->core->getSelection(&pg, &ulx, &uly, &lrx, &lry)) {
    viewer->core->zoomToRect(pg, ulx, uly, lrx, lry);
  }
}

void XPDFViewer::closeCbk(Widget widget, XtPointer ptr,
			  XtPointer callData) {
  XPDFViewer *viewer = (XPDFViewer *)ptr;

  viewer->app->close(viewer, false);
}

void XPDFViewer::closeMsgCbk(Widget widget, XtPointer ptr,
			     XtPointer callData) {
  XPDFViewer *viewer = (XPDFViewer *)ptr;

  viewer->app->close(viewer, true);
}

void XPDFViewer::pageNumCbk(Widget widget, XtPointer ptr,
			    XtPointer callData) {
  XPDFViewer *viewer = (XPDFViewer *)ptr;
  char *s, *p;
  int pg;
  char buf[20];

  if (!viewer->core->getDoc()) {
    goto err;
  }
  s = XmTextFieldGetString(viewer->pageNumText);
  for (p = s; *p; ++p) {
    if (!isdigit(*p)) {
      goto err;
    }
  }
  pg = atoi(s);
  if (pg < 1 || pg > viewer->core->getDoc()->getNumPages()) {
    goto err;
  }
  viewer->displayPage(pg, viewer->core->getZoom(),
		      viewer->core->getRotate(), false, true);
  viewer->core->takeFocus();
  return;

 err:
  XBell(viewer->display, 0);
  snprintf(buf, sizeof buf, "%d", viewer->core->getPageNum());
  XmTextFieldSetString(viewer->pageNumText, buf);
}

void XPDFViewer::updateCbk(void *data, PCONST GooString *fileName,
			   int pageNum, int numPages, const char *linkString) {
  XPDFViewer *viewer = (XPDFViewer *)data;
  char buf[20];
  XmString s;

  if (fileName) {
    std::string title = viewer->app->getTitle();
    if (title == "") {
      title = std::string(xpdfAppName ": ") + fileName->getCString();
    }
    XtVaSetValues(viewer->win, XmNtitle, title.c_str(),
		  XmNiconName, title.c_str(), NULL);
#ifndef DISABLE_OUTLINE
    viewer->setupOutline();
#endif
    viewer->setupPrintDialog();
  }

  if (viewer->toolBar != None) {
    if (pageNum >= 0) {
      s = XmStringCreateLocalized("");
      XtVaSetValues(viewer->linkLabel, XmNlabelString, s, NULL);
      XmStringFree(s);
      snprintf(buf, sizeof buf, "%d", pageNum);
      XmTextFieldSetString(viewer->pageNumText, buf);
      XtVaSetValues(viewer->prevTenPageBtn, XmNsensitive,
		    pageNum > 1, NULL);
      XtVaSetValues(viewer->prevPageBtn, XmNsensitive,
		    pageNum > 1, NULL);
      XtVaSetValues(viewer->nextTenPageBtn, XmNsensitive,
		    pageNum < viewer->core->getDoc()->getNumPages(), NULL);
      XtVaSetValues(viewer->nextPageBtn, XmNsensitive,
		    pageNum < viewer->core->getDoc()->getNumPages(), NULL);
      XtVaSetValues(viewer->backBtn, XmNsensitive,
		    viewer->core->canGoBack(), NULL);
      XtVaSetValues(viewer->forwardBtn, XmNsensitive,
		    viewer->core->canGoForward(), NULL);
    }

    if (numPages >= 0) {
      snprintf(buf, sizeof buf, " of %d", numPages);
      s = XmStringCreateLocalized(buf);
      XtVaSetValues(viewer->pageCountLabel, XmNlabelString, s, NULL);
      XmStringFree(s);
    }

    if (linkString) {
      s = XmStringCreateLocalized((char *)linkString);
      XtVaSetValues(viewer->linkLabel, XmNlabelString, s, NULL);
      XmStringFree(s);
    }
  }
}

//------------------------------------------------------------------------
// GUI code: outline
//------------------------------------------------------------------------

#ifndef DISABLE_OUTLINE

void XPDFViewer::setupOutline() {
  PCONST OutlineItemList *items;
  int i;

  if (outlineScroll == None) {
    return;
  }

  // unmanage and destroy the old labels
  if (outlineLabels) {
    XtUnmanageChildren(outlineLabels, outlineLabelsLength);
    for (i = 0; i < outlineLabelsLength; ++i) {
      XtDestroyWidget(outlineLabels[i]);
    }
    gfree(outlineLabels);
    outlineLabels = NULL;
    outlineLabelsLength = outlineLabelsSize = 0;
  }

  if (core->getDoc()) {

    // create the new labels
    items = core->getDoc()->getOutline()->getItems();
    if (items && items->getOILSize() > 0) {
      setupOutlineItems(items, NULL);
    }

    // manage the new labels
    XtManageChildren(outlineLabels, outlineLabelsLength);
  }
}

void XPDFViewer::setupOutlineItems(PCONST OutlineItemList *items,
				   Widget parent) {
  OutlineItem *item;
  PCONST OutlineItemList *kids;
  Widget label;
  Arg args[20];
  GooString *title;
  char buf[8];
  XmString s;
  int i, j, n;

#ifdef UNICODEMAP_NO_REFCOUNT
  const UnicodeMap *uMap = globalParams->getUnicodeMap("Latin1");
#else
  GooString enc("Latin1");
  UnicodeMap *uMap = globalParams->getUnicodeMap(&enc);
#endif

  for (i = 0; i < (int)items->getOILSize(); ++i) {
#ifdef NO_GOOLIST
    item = (*items)[i];
#else
    item = (OutlineItem *)items->get(i);
#endif
    title = new GooString();
    for (j = 0; j < item->getTitleLength(); ++j) {
      n = uMap->mapUnicode(item->getTitle()[j], buf, sizeof(buf));
      title->append(buf, n);
    }
    n = 0;
    XtSetArg(args[n], XPDFNentryPosition, i); ++n;
    if (parent) {
      XtSetArg(args[n], XPDFNentryParent, parent); ++n;
    }
    XtSetArg(args[n], XPDFNentryExpanded, item->isOpen()); ++n;
    s = XmStringCreateLocalized((char *)title->getCString());
    delete title;
    XtSetArg(args[n], XmNlabelString, s); ++n;
    XtSetArg(args[n], XmNuserData, item); ++n;
    XtSetArg(args[n], XmNmarginWidth, 0); ++n;
    XtSetArg(args[n], XmNmarginHeight, 2); ++n;
    XtSetArg(args[n], XmNshadowThickness, 0); ++n;
    XtSetArg(args[n], XmNforeground,
	     app->getReverseVideo() ? WhitePixel(display, screenNum)
	                            : BlackPixel(display, screenNum)); ++n;
    XtSetArg(args[n], XmNbackground, app->getPaperPixel()); ++n;
    label = XmCreateLabelGadget(outlineTree, "label", args, n);
    XmStringFree(s);
    if (outlineLabelsLength == outlineLabelsSize) {
      outlineLabelsSize += 64;
      outlineLabels = (Widget *)greallocn(outlineLabels,
					  outlineLabelsSize, sizeof(Widget *));
    }
    outlineLabels[outlineLabelsLength++] = label;
    item->open();
    if ((kids = item->getKids())) {
      setupOutlineItems(kids, label);
    }
  }

#ifndef UNICODEMAP_NO_REFCOUNT
  uMap->decRefCnt();
#endif
}

void XPDFViewer::outlineSelectCbk(Widget widget, XtPointer ptr,
				  XtPointer callData) {
  XPDFViewer *viewer = (XPDFViewer *)ptr;
  XPDFTreeSelectCallbackStruct *data =
      (XPDFTreeSelectCallbackStruct *)callData;
  OutlineItem *item;

  XtVaGetValues(data->selectedItem, XmNuserData, &item, NULL);
  if (item) {
    if (item->getAction()) {
      viewer->core->doAction(item->getAction());
    }
  }
  viewer->core->takeFocus();
}

#endif // !DISABLE_OUTLINE

//------------------------------------------------------------------------
// GUI code: "about" dialog
//------------------------------------------------------------------------

void XPDFViewer::initAboutDialog() {
  Widget scrolledWin, col, label, sep, closeBtn;
  Arg args[20];
  int n, i;
  XmString s;
  char buf[20];

  //----- dialog
  n = 0;
  s = XmStringCreateLocalized(xpdfAppName ": About");
  XtSetArg(args[n], XmNdialogTitle, s); ++n;
  XtSetArg(args[n], XmNwidth, 450); ++n;
  XtSetArg(args[n], XmNheight, 300); ++n;
  aboutDialog = XmCreateFormDialog(win, "aboutDialog", args, n);
  XmStringFree(s);

  //----- "close" button
  n = 0;
  XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNrightOffset, 4); ++n;
  XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNbottomOffset, 4); ++n;
  closeBtn = XmCreatePushButton(aboutDialog, "Close", args, n);
  XtManageChild(closeBtn);
  n = 0;
  XtSetArg(args[n], XmNdefaultButton, closeBtn); ++n;
  XtSetArg(args[n], XmNcancelButton, closeBtn); ++n;
  XtSetValues(aboutDialog, args, n);

  //----- scrolled window and RowColumn
  n = 0;
  XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); ++n;
  XtSetArg(args[n], XmNbottomWidget, closeBtn); ++n;
  XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNscrollingPolicy, XmAUTOMATIC); ++n;
  scrolledWin = XmCreateScrolledWindow(aboutDialog, "scrolledWin", args, n);
  XtManageChild(scrolledWin);
  n = 0;
  XtSetArg(args[n], XmNorientation, XmVERTICAL); ++n;
  XtSetArg(args[n], XmNpacking, XmPACK_TIGHT); ++n;
  col = XmCreateRowColumn(scrolledWin, "col", args, n);
  XtManageChild(col);

  //----- fonts
  aboutBigFont =
    createFontList("-*-times-bold-i-normal--20-*-*-*-*-*-iso10646-1");
  aboutVersionFont =
    createFontList("-*-times-medium-r-normal--16-*-*-*-*-*-iso10646-1");
  aboutFixedFont =
    createFontList("-*-courier-medium-r-normal--12-*-*-*-*-*-iso10646-1");

  //----- heading
  n = 0;
  s = XmStringCreateLocalized("xpopple");
  XtSetArg(args[n], XmNlabelString, s); ++n;
  XtSetArg(args[n], XmNfontList, aboutBigFont); ++n;
  label = XmCreateLabel(col, "h0", args, n);
  XmStringFree(s);
  XtManageChild(label);
  n = 0;
  s = XmStringCreateLocalized("Version " xpdfVersion);
  XtSetArg(args[n], XmNlabelString, s); ++n;
  XtSetArg(args[n], XmNfontList, aboutVersionFont); ++n;
  label = XmCreateLabel(col, "h1", args, n);
  XmStringFree(s);
  XtManageChild(label);
  n = 0;
  s = XmStringCreateLocalized(xpdfCopyright);
  XtSetArg(args[n], XmNlabelString, s); ++n;
  XtSetArg(args[n], XmNfontList, aboutVersionFont); ++n;
  label = XmCreateLabel(col, "h2", args, n);
  XmStringFree(s);
  XtManageChild(label);
  n = 0;
  s = XmStringCreateLocalized(" ");
  XtSetArg(args[n], XmNlabelString, s); ++n;
  XtSetArg(args[n], XmNfontList, aboutVersionFont); ++n;
  label = XmCreateLabel(col, "h3", args, n);
  XmStringFree(s);
  XtManageChild(label);
  n = 0;
  XtSetArg(args[n], XmNorientation, XmHORIZONTAL); ++n;
  sep = XmCreateSeparator(col, "sep", args, n);
  XtManageChild(sep);
  n = 0;
  s = XmStringCreateLocalized(" ");
  XtSetArg(args[n], XmNlabelString, s); ++n;
  XtSetArg(args[n], XmNfontList, aboutVersionFont); ++n;
  label = XmCreateLabel(col, "h4", args, n);
  XmStringFree(s);
  XtManageChild(label);

  //----- text
  for (i = 0; aboutWinText[i]; ++i) {
    n = 0;
    s = XmStringCreateLocalized((char *)aboutWinText[i]);
    XtSetArg(args[n], XmNlabelString, s); ++n;
    XtSetArg(args[n], XmNfontList, aboutFixedFont); ++n;
    snprintf(buf, sizeof buf, "t%d", i);
    label = XmCreateLabel(col, buf, args, n);
    XtManageChild(label);
    XmStringFree(s);
  }
}

//------------------------------------------------------------------------
// GUI code: "open" dialog
//------------------------------------------------------------------------

void XPDFViewer::initOpenDialog() {
  Arg args[20];
  int n;
  XmString s1, s2, s3;
  GooString *dir;

  n = 0;
  s1 = XmStringCreateLocalized("Open");
  XtSetArg(args[n], XmNokLabelString, s1); ++n;
  s2 = XmStringCreateLocalized("*.[Pp][Dd][Ff]");
  XtSetArg(args[n], XmNpattern, s2); ++n;
  s3 = XmStringCreateLocalized(xpdfAppName ": Open");
  XtSetArg(args[n], XmNdialogTitle, s3); ++n;
  XtSetArg(args[n], XmNdialogStyle, XmDIALOG_PRIMARY_APPLICATION_MODAL); ++n;
  XtSetArg(args[n], XmNautoUnmanage, True); ++n;
  openDialog = XmCreateFileSelectionDialog(win, "openDialog", args, n);
  XmStringFree(s1);
  XmStringFree(s2);
  XmStringFree(s3);
  XtUnmanageChild(XmFileSelectionBoxGetChild(openDialog,
					     XmDIALOG_HELP_BUTTON));
  XtAddCallback(openDialog, XmNokCallback,
		&openOkCbk, (XtPointer)this);

  if (core->getDoc() && core->getDoc()->getFileName()) {
    dir = makePathAbsolute(xpdfGrabPath(
	      core->getDoc()->getFileName()->getCString()));
    s1 = XmStringCreateLocalized((char *)dir->getCString());
    XtVaSetValues(openDialog, XmNdirectory, s1, NULL);
    XmStringFree(s1);
    delete dir;
  }
}

void XPDFViewer::mapOpenDialog(bool openInNewWindowA) {
  if (!openDialog) {
    initOpenDialog();
  }
  openInNewWindow = openInNewWindowA;
  XmFileSelectionDoSearch(openDialog, NULL);
  XtManageChild(openDialog);
}

void XPDFViewer::openOkCbk(Widget widget, XtPointer ptr,
			   XtPointer callData) {
  XPDFViewer *viewer = (XPDFViewer *)ptr;
  XmFileSelectionBoxCallbackStruct *data =
    (XmFileSelectionBoxCallbackStruct *)callData;
  char *fileName;
  XmStringContext context;
  XmStringCharSet charSet;
  XmStringDirection dir;
  Boolean sep;

  XmStringInitContext(&context, data->value);
  if (XmStringGetNextSegment(context, &fileName, &charSet, &dir, &sep)) {
    std::string fileNameStr(fileName);
    if (viewer->openInNewWindow) {
      viewer->app->open(&fileNameStr);
    } else {
      if (viewer->loadFile(fileNameStr)) {
	viewer->displayPage(1, viewer->core->getZoom(),
			    viewer->core->getRotate(), true, true);
      }
    }
    XtFree(charSet);
    XtFree(fileName);
  }
  XmStringFreeContext(context);
}

//------------------------------------------------------------------------
// GUI code: "find" dialog
//------------------------------------------------------------------------

void XPDFViewer::initFindDialog() {
  Widget form1, label, okBtn, closeBtn;
  Arg args[20];
  int n;
  XmString s;

  //----- dialog
  n = 0;
  s = XmStringCreateLocalized(xpdfAppName ": Find");
  XtSetArg(args[n], XmNdialogTitle, s); ++n;
  XtSetArg(args[n], XmNnavigationType, XmNONE); ++n;
  XtSetArg(args[n], XmNautoUnmanage, False); ++n;
  findDialog = XmCreateFormDialog(win, "findDialog", args, n);
  XmStringFree(s);

  //----- "find" and "close" buttons
  n = 0;
  XtSetArg(args[n], XmNtraversalOn, false); ++n;
  XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNleftOffset, 4); ++n;
  XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNbottomOffset, 4); ++n;
  XtSetArg(args[n], XmNnavigationType, XmEXCLUSIVE_TAB_GROUP); ++n;
  okBtn = XmCreatePushButton(findDialog, "Find", args, n);
  XtManageChild(okBtn);
  XtAddCallback(okBtn, XmNactivateCallback,
		&findFindCbk, (XtPointer)this);
  n = 0;
  XtSetArg(args[n], XmNtraversalOn, false); ++n;
  XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNrightOffset, 4); ++n;
  XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNbottomOffset, 4); ++n;
  XtSetArg(args[n], XmNnavigationType, XmEXCLUSIVE_TAB_GROUP); ++n;
  closeBtn = XmCreatePushButton(findDialog, "Close", args, n);
  XtManageChild(closeBtn);
  XtAddCallback(closeBtn, XmNactivateCallback,
		&findCloseCbk, (XtPointer)this);

  //----- case sensitiveness checkbox
  n = 0;
  XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNleftOffset, 2); ++n;
  XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); ++n;
  XtSetArg(args[n], XmNbottomWidget, okBtn); ++n;
  XtSetArg(args[n], XmNindicatorType, XmN_OF_MANY); ++n;
#if XmVERSION <= 1
  XtSetArg(args[n], XmNindicatorOn, True); ++n;
#else
  XtSetArg(args[n], XmNindicatorOn, XmINDICATOR_FILL); ++n;
#endif
  XtSetArg(args[n], XmNset, XmUNSET); ++n;
  s = XmStringCreateLocalized("Match case");
  XtSetArg(args[n], XmNlabelString, s); ++n;
  findCaseSensitiveToggle =
      XmCreateToggleButton(findDialog, "matchCase", args, n);
  XmStringFree(s);
  XtManageChild(findCaseSensitiveToggle);

  //----- search string entry
  n = 0;
  XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNtopOffset, 4); ++n;
  XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); ++n;
  XtSetArg(args[n], XmNbottomWidget, findCaseSensitiveToggle); ++n;
  XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNleftOffset, 2); ++n;
  XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNrightOffset, 2); ++n;
  form1 = XmCreateForm(findDialog, "form", args, n);
  XtManageChild(form1);
  n = 0;
  XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); ++n;
  s = XmStringCreateLocalized("Find text: ");
  XtSetArg(args[n], XmNlabelString, s); ++n;
  label = XmCreateLabel(form1, "label", args, n);
  XmStringFree(s);
  XtManageChild(label);
  n = 0;
  XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); ++n;
  XtSetArg(args[n], XmNleftWidget, label); ++n;
  XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); ++n;
  findText = XmCreateTextField(form1, "text", args, n);
  XtManageChild(findText);
#ifdef LESSTIF_VERSION
  XtAddCallback(findText, XmNactivateCallback,
		&findFindCbk, (XtPointer)this);
#endif

  //----- dialog parameters
  n = 0;
  XtSetArg(args[n], XmNdefaultButton, okBtn); ++n;
  XtSetArg(args[n], XmNcancelButton, closeBtn); ++n;
#if XmVersion > 1001
  XtSetArg(args[n], XmNinitialFocus, findText); ++n;
#endif
  XtSetValues(findDialog, args, n);
}

void XPDFViewer::findFindCbk(Widget widget, XtPointer ptr,
			     XtPointer callData) {
  XPDFViewer *viewer = (XPDFViewer *)ptr;

  viewer->doFind(false, 0);
  XtUnmanageChild(viewer->findDialog);
}

void XPDFViewer::mapFindDialog() {
  XmTextFieldSetSelection(findText, 0, XmTextFieldGetLastPosition(findText),
			  XtLastTimestampProcessed(display));
  XmTextFieldSetInsertionPosition(findText, 0);
  XtManageChild(findDialog);
}

void XPDFViewer::doFind(bool next, int backward) {
  if (XtWindow(findDialog)) {
    XDefineCursor(display, XtWindow(findDialog), core->getBusyCursor());
  }
  core->find(XmTextFieldGetString(findText),
	     XmToggleButtonGetState(findCaseSensitiveToggle),
	     next,
	     backward,
	     0,
	     false);
  if (XtWindow(findDialog)) {
    XUndefineCursor(display, XtWindow(findDialog));
  }
}

void XPDFViewer::findCloseCbk(Widget widget, XtPointer ptr,
			      XtPointer callData) {
  XPDFViewer *viewer = (XPDFViewer *)ptr;

  XtUnmanageChild(viewer->findDialog);
}

//------------------------------------------------------------------------
// GUI code: "save as" dialog
//------------------------------------------------------------------------

void XPDFViewer::initSaveAsDialog() {
  Arg args[20];
  int n;
  XmString s1, s2, s3;
  GooString *dir;

  n = 0;
  s1 = XmStringCreateLocalized("Save");
  XtSetArg(args[n], XmNokLabelString, s1); ++n;
  s2 = XmStringCreateLocalized("*.[Pp][Dd][Ff]");
  XtSetArg(args[n], XmNpattern, s2); ++n;
  s3 = XmStringCreateLocalized(xpdfAppName ": Save as");
  XtSetArg(args[n], XmNdialogTitle, s3); ++n;
  XtSetArg(args[n], XmNdialogStyle, XmDIALOG_PRIMARY_APPLICATION_MODAL); ++n;
  XtSetArg(args[n], XmNautoUnmanage, True); ++n;
  saveAsDialog = XmCreateFileSelectionDialog(win, "saveAsDialog", args, n);
  XmStringFree(s1);
  XmStringFree(s2);
  XmStringFree(s3);
  XtUnmanageChild(XmFileSelectionBoxGetChild(saveAsDialog,
					     XmDIALOG_HELP_BUTTON));
  XtAddCallback(saveAsDialog, XmNokCallback,
		&saveAsOkCbk, (XtPointer)this);

  if (core->getDoc() && core->getDoc()->getFileName()) {
    dir = makePathAbsolute(xpdfGrabPath(
	      core->getDoc()->getFileName()->getCString()));
    s1 = XmStringCreateLocalized((char *)dir->getCString());
    XtVaSetValues(saveAsDialog, XmNdirectory, s1, NULL);
    XmStringFree(s1);
    delete dir;
  }
}

void XPDFViewer::mapSaveAsDialog() {
  if (!saveAsDialog) {
    initSaveAsDialog();
  }
  XmFileSelectionDoSearch(saveAsDialog, NULL);
  XtManageChild(saveAsDialog);
}

void XPDFViewer::saveAsOkCbk(Widget widget, XtPointer ptr,
			     XtPointer callData) {
  XPDFViewer *viewer = (XPDFViewer *)ptr;
  XmFileSelectionBoxCallbackStruct *data =
    (XmFileSelectionBoxCallbackStruct *)callData;
  char *fileName;
  GooString *fileNameStr;
  XmStringContext context;
  XmStringCharSet charSet;
  XmStringDirection dir;
  Boolean sep;

  XmStringInitContext(&context, data->value);
  if (XmStringGetNextSegment(context, &fileName, &charSet, &dir, &sep)) {
    fileNameStr = new GooString(fileName);
    viewer->core->getDoc()->saveAs(fileNameStr);
    delete fileNameStr;
    XtFree(charSet);
    XtFree(fileName);
  }
  XmStringFreeContext(context);
}

//------------------------------------------------------------------------
// GUI code: "print" dialog
//------------------------------------------------------------------------

void XPDFViewer::initPrintDialog() {
  Widget sep1, sep2, sep3, row, label1, label2, okBtn, cancelBtn;
  Arg args[20];
  int n;
  XmString s;
  std::string psFileName;

  //----- dialog
  n = 0;
  s = XmStringCreateLocalized(xpdfAppName ": Print");
  XtSetArg(args[n], XmNdialogTitle, s); ++n;
  XtSetArg(args[n], XmNdialogStyle, XmDIALOG_PRIMARY_APPLICATION_MODAL); ++n;
  printDialog = XmCreateFormDialog(win, "printDialog", args, n);
  XmStringFree(s);

  //----- "print with command"
  n = 0;
  XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNtopOffset, 4); ++n;
  XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNindicatorType, XmONE_OF_MANY); ++n;
  XtSetArg(args[n], XmNset, XmSET); ++n;
  s = XmStringCreateLocalized("Print with command:");
  XtSetArg(args[n], XmNlabelString, s); ++n;
  printWithCmdBtn = XmCreateToggleButton(printDialog, "printWithCmd", args, n);
  XmStringFree(s);
  XtManageChild(printWithCmdBtn);
  XtAddCallback(printWithCmdBtn, XmNvalueChangedCallback,
		&printWithCmdBtnCbk, (XtPointer)this);
  n = 0;
  XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); ++n;
  XtSetArg(args[n], XmNtopWidget, printWithCmdBtn); ++n;
  XtSetArg(args[n], XmNtopOffset, 2); ++n;
  XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNleftOffset, 16); ++n;
  XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNrightOffset, 4); ++n;
  XtSetArg(args[n], XmNcolumns, 40); ++n;
  printCmdText = XmCreateTextField(printDialog, "printCmd", args, n);
  XtManageChild(printCmdText);

  //----- "print to file"
  n = 0;
  XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); ++n;
  XtSetArg(args[n], XmNtopWidget, printCmdText); ++n;
  XtSetArg(args[n], XmNtopOffset, 4); ++n;
  XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNindicatorType, XmONE_OF_MANY); ++n;
  XtSetArg(args[n], XmNset, XmUNSET); ++n;
  s = XmStringCreateLocalized("Print to file:");
  XtSetArg(args[n], XmNlabelString, s); ++n;
  printToFileBtn = XmCreateToggleButton(printDialog, "printToFile", args, n);
  XmStringFree(s);
  XtManageChild(printToFileBtn);
  XtAddCallback(printToFileBtn, XmNvalueChangedCallback,
		&printToFileBtnCbk, (XtPointer)this);
  n = 0;
  XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); ++n;
  XtSetArg(args[n], XmNtopWidget, printToFileBtn); ++n;
  XtSetArg(args[n], XmNtopOffset, 2); ++n;
  XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNleftOffset, 16); ++n;
  XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNrightOffset, 4); ++n;
  XtSetArg(args[n], XmNcolumns, 40); ++n;
  XtSetArg(args[n], XmNsensitive, False); ++n;
  printFileText = XmCreateTextField(printDialog, "printFile", args, n);
  XtManageChild(printFileText);

  //----- separator
  n = 0;
  XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); ++n;
  XtSetArg(args[n], XmNtopWidget, printFileText); ++n;
  XtSetArg(args[n], XmNtopOffset, 8); ++n;
  XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNleftOffset, 8); ++n;
  XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNrightOffset, 8); ++n;
  XtSetArg(args[n], XmNorientation, XmHORIZONTAL); ++n;
  sep1 = XmCreateSeparator(printDialog, "sep1", args, n);
  XtManageChild(sep1);

  //----- page range
  n = 0;
  XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); ++n;
  XtSetArg(args[n], XmNtopWidget, sep1); ++n;
  XtSetArg(args[n], XmNtopOffset, 8); ++n;
  XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNleftOffset, 4); ++n;
  XtSetArg(args[n], XmNorientation, XmHORIZONTAL); ++n;
  XtSetArg(args[n], XmNpacking, XmPACK_TIGHT); ++n;
  row = XmCreateRowColumn(printDialog, "row", args, n);
  XtManageChild(row);
  n = 0;
  s = XmStringCreateLocalized("Pages:");
  XtSetArg(args[n], XmNlabelString, s); ++n;
  label1 = XmCreateLabel(row, "label1", args, n);
  XmStringFree(s);
  XtManageChild(label1);
  n = 0;
  XtSetArg(args[n], XmNcolumns, 5); ++n;
  printFirstPage = XmCreateTextField(row, "printFirstPage", args, n);
  XtManageChild(printFirstPage);
  n = 0;
  s = XmStringCreateLocalized("to");
  XtSetArg(args[n], XmNlabelString, s); ++n;
  label2 = XmCreateLabel(row, "label2", args, n);
  XmStringFree(s);
  XtManageChild(label2);
  n = 0;
  XtSetArg(args[n], XmNcolumns, 5); ++n;
  printLastPage = XmCreateTextField(row, "printLastPage", args, n);
  XtManageChild(printLastPage);

  //----- separator
  n = 0;
  XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); ++n;
  XtSetArg(args[n], XmNtopWidget, row); ++n;
  XtSetArg(args[n], XmNtopOffset, 8); ++n;
  XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNleftOffset, 8); ++n;
  XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNrightOffset, 8); ++n;
  XtSetArg(args[n], XmNorientation, XmHORIZONTAL); ++n;
  sep2 = XmCreateSeparator(printDialog, "sep2", args, n);
  XtManageChild(sep2);

  //----- Print All Pages
  n = 0;
  XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); ++n;
  XtSetArg(args[n], XmNtopWidget, sep2); ++n;
  XtSetArg(args[n], XmNtopOffset, 8); ++n;
  XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNleftOffset, 8); ++n;
  XtSetArg(args[n], XmNindicatorType, XmONE_OF_MANY); ++n;
  XtSetArg(args[n], XmNset, XmSET); ++n;
  s = XmStringCreateLocalized("Print all pages");
  XtSetArg(args[n], XmNlabelString, s); ++n;
  printAllPages = XmCreateToggleButton(printDialog, "printAllPages", args, n);
  XmStringFree(s);
  XtManageChild(printAllPages);
  XtAddCallback(printAllPages, XmNvalueChangedCallback,
    &printAllPagesBtnCbk, (XtPointer)this);

  //----- Print Odd Pages
  n = 0;
  XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); ++n;
  XtSetArg(args[n], XmNtopWidget, printAllPages); ++n;
//   XtSetArg(args[n], XmNtopOffset, 4); ++n;
  XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNleftOffset, 8); ++n;
  XtSetArg(args[n], XmNindicatorType, XmONE_OF_MANY); ++n;
  XtSetArg(args[n], XmNset, XmUNSET); ++n;
  s = XmStringCreateLocalized("Print odd pages");
  XtSetArg(args[n], XmNlabelString, s); ++n;
  printOddPages = XmCreateToggleButton(printDialog, "printOddPages", args, n);
  XmStringFree(s);
  XtManageChild(printOddPages);
  XtAddCallback(printOddPages, XmNvalueChangedCallback,
    &printOddPagesBtnCbk, (XtPointer)this);

  //----- Print Even Pages
  n = 0;
  XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); ++n;
  XtSetArg(args[n], XmNtopWidget, printOddPages); ++n;
//   XtSetArg(args[n], XmNtopOffset, 4); ++n;
  XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNleftOffset, 8); ++n;
  XtSetArg(args[n], XmNindicatorType, XmONE_OF_MANY); ++n;
  XtSetArg(args[n], XmNset, XmUNSET); ++n;
  s = XmStringCreateLocalized("Print even pages");
  XtSetArg(args[n], XmNlabelString, s); ++n;
  printEvenPages = XmCreateToggleButton(printDialog, "printEvenPages", args, n);
  XmStringFree(s);
  XtManageChild(printEvenPages);
  XtAddCallback(printEvenPages, XmNvalueChangedCallback,
    &printEvenPagesBtnCbk, (XtPointer)this);

  //----- separator
  n = 0;
  XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); ++n;
  XtSetArg(args[n], XmNtopWidget, printEvenPages); ++n;
  XtSetArg(args[n], XmNtopOffset, 8); ++n;
  XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNleftOffset, 8); ++n;
  XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNrightOffset, 8); ++n;
  XtSetArg(args[n], XmNorientation, XmHORIZONTAL); ++n;
  sep3 = XmCreateSeparator(printDialog, "sep3", args, n);
  XtManageChild(sep3);

  //----- Print Back Order
  n = 0;
  XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); ++n;
  XtSetArg(args[n], XmNtopWidget, sep3); ++n;
  XtSetArg(args[n], XmNtopOffset, 8); ++n;
  XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNleftOffset, 8); ++n;
  XtSetArg(args[n], XmNindicatorType, XmONE_OF_MANY); ++n;
  XtSetArg(args[n], XmNset, XmUNSET); ++n;
  s = XmStringCreateLocalized("Print back order");
  XtSetArg(args[n], XmNlabelString, s); ++n;
  printBackOrder = XmCreateToggleButton(printDialog, "printBackOrder", args, n);
  XmStringFree(s);
  XtManageChild(printBackOrder);
  XtAddCallback(printBackOrder, XmNvalueChangedCallback,
    &printBackOrderBtnCbk, (XtPointer)this);

  //----- separator
  n = 0;
  XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); ++n;
  XtSetArg(args[n], XmNtopWidget, printBackOrder); ++n;
  XtSetArg(args[n], XmNtopOffset, 8); ++n;
  XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNleftOffset, 8); ++n;
  XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNrightOffset, 8); ++n;
  XtSetArg(args[n], XmNorientation, XmHORIZONTAL); ++n;
  sep3 = XmCreateSeparator(printDialog, "sep3", args, n);
  XtManageChild(sep3);

  //----- "print" and "cancel" buttons
  n = 0;
  XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); ++n;
  XtSetArg(args[n], XmNtopWidget, sep3); ++n;
  XtSetArg(args[n], XmNtopOffset, 8); ++n;
  XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNleftOffset, 4); ++n;
  XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNbottomOffset, 4); ++n;
  okBtn = XmCreatePushButton(printDialog, "Print", args, n);
  XtManageChild(okBtn);
  XtAddCallback(okBtn, XmNactivateCallback,
		&printPrintCbk, (XtPointer)this);
  n = 0;
  XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); ++n;
  XtSetArg(args[n], XmNtopWidget, sep3); ++n;
  XtSetArg(args[n], XmNtopOffset, 8); ++n;
  XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNrightOffset, 4); ++n;
  XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); ++n;
  XtSetArg(args[n], XmNbottomOffset, 4); ++n;
  cancelBtn = XmCreatePushButton(printDialog, "Cancel", args, n);
  XtManageChild(cancelBtn);
  n = 0;
  XtSetArg(args[n], XmNdefaultButton, okBtn); ++n;
  XtSetArg(args[n], XmNcancelButton, cancelBtn); ++n;
  XtSetValues(printDialog, args, n);

  //----- initial values

  psFileName = xpdfParams->getPSFile();
  if (psFileName != "") {
    if (psFileName[0] == '|') {
      XmTextFieldSetString(printCmdText,
			   (char *)psFileName.c_str() + 1);
    } else {
      XmTextFieldSetString(printFileText, (char *)psFileName.c_str());
    }
  }
}

void XPDFViewer::printAllPagesBtnCbk(Widget widget,
  XtPointer ptr, XtPointer callData)
{
  XPDFViewer *viewer = (XPDFViewer *)ptr;
  XmToggleButtonCallbackStruct *data =
      (XmToggleButtonCallbackStruct *)callData;

  if (data->set != XmSET) {
    XmToggleButtonSetState(viewer->printAllPages, True, False);
  }
  XmToggleButtonSetState(viewer->printEvenPages, False, False);
  XmToggleButtonSetState(viewer->printOddPages, False, False);
}

void XPDFViewer::printEvenPagesBtnCbk(Widget widget,
  XtPointer ptr, XtPointer callData)
{
  XPDFViewer *viewer = (XPDFViewer *)ptr;
  XmToggleButtonCallbackStruct *data =
      (XmToggleButtonCallbackStruct *)callData;

  if (data->set != XmSET) {
    XmToggleButtonSetState(viewer->printEvenPages, True, False);
  }
  XmToggleButtonSetState(viewer->printAllPages, False, False);
  XmToggleButtonSetState(viewer->printOddPages, False, False);
  XmToggleButtonSetState(viewer->printWithCmdBtn, True, False);
  XmToggleButtonSetState(viewer->printToFileBtn, False, False);
}

void XPDFViewer::printOddPagesBtnCbk(Widget widget,
  XtPointer ptr, XtPointer callData)
{
  XPDFViewer *viewer = (XPDFViewer *)ptr;
  XmToggleButtonCallbackStruct *data =
      (XmToggleButtonCallbackStruct *)callData;

  if (data->set != XmSET) {
    XmToggleButtonSetState(viewer->printOddPages, True, False);
  }
  XmToggleButtonSetState(viewer->printAllPages, False, False);
  XmToggleButtonSetState(viewer->printEvenPages, False, False);
  XmToggleButtonSetState(viewer->printWithCmdBtn, True, False);
  XmToggleButtonSetState(viewer->printToFileBtn, False, False);
}

void XPDFViewer::printBackOrderBtnCbk(Widget widget, XtPointer ptr,
  XtPointer callData)
{
  XPDFViewer *viewer = (XPDFViewer *)ptr;
  XmToggleButtonSetState(viewer->printWithCmdBtn, True, False);
  XmToggleButtonSetState(viewer->printToFileBtn, False, False);
}

void XPDFViewer::setupPrintDialog() {
  PDFDoc *doc;
  char buf[20];
  std::string psFileName2;
  const char *p;

  doc = core->getDoc();
  auto pdfFileName = doc->getFileName();
  auto psFileName = xpdfParams->getPSFile();
  if (psFileName == "" || psFileName[0] == '|') {
    if ((pdfFileName = doc->getFileName())) {
      p = pdfFileName->getCString() + pdfFileName->getLength() - 4;
      if (!strcmp(p, ".pdf") || !strcmp(p, ".PDF")) {
	psFileName2 = std::string(pdfFileName->getCString(),
				  pdfFileName->getLength() - 4);
      } else {
	psFileName2 = toString(pdfFileName);
      }
      psFileName2.append(".ps");
      XmTextFieldSetString(printFileText, (char *)psFileName2.c_str());
    }
  }
  if (psFileName != "" && psFileName[0] == '|') {
    XmToggleButtonSetState(printWithCmdBtn, True, False);
    XmToggleButtonSetState(printToFileBtn, False, False);
    XtVaSetValues(printCmdText, XmNsensitive, True, NULL);
    XtVaSetValues(printFileText, XmNsensitive, False, NULL);
  } else {
    XmToggleButtonSetState(printWithCmdBtn, False, False);
    XmToggleButtonSetState(printToFileBtn, True, False);
    XtVaSetValues(printCmdText, XmNsensitive, False, NULL);
    XtVaSetValues(printFileText, XmNsensitive, True, NULL);
  }

  snprintf(buf, sizeof buf, "%d", doc->getNumPages());
  XmTextFieldSetString(printFirstPage, "1");
  XmTextFieldSetString(printLastPage, buf);
}

void XPDFViewer::printWithCmdBtnCbk(Widget widget, XtPointer ptr,
				    XtPointer callData) {
  XPDFViewer *viewer = (XPDFViewer *)ptr;
  XmToggleButtonCallbackStruct *data =
      (XmToggleButtonCallbackStruct *)callData;

  if (data->set != XmSET) {
    XmToggleButtonSetState(viewer->printWithCmdBtn, True, False);
  }
  XmToggleButtonSetState(viewer->printToFileBtn, False, False);
  XtVaSetValues(viewer->printCmdText, XmNsensitive, True, NULL);
  XtVaSetValues(viewer->printFileText, XmNsensitive, False, NULL);
}

void XPDFViewer::printToFileBtnCbk(Widget widget, XtPointer ptr,
				   XtPointer callData) {
  XPDFViewer *viewer = (XPDFViewer *)ptr;
  XmToggleButtonCallbackStruct *data =
      (XmToggleButtonCallbackStruct *)callData;

  if (data->set != XmSET) {
    XmToggleButtonSetState(viewer->printToFileBtn, True, False);
  }
  XmToggleButtonSetState(viewer->printWithCmdBtn, False, False);
  XtVaSetValues(viewer->printFileText, XmNsensitive, True, NULL);
  XtVaSetValues(viewer->printCmdText, XmNsensitive, False, NULL);

  XmToggleButtonSetState(viewer->printAllPages, True, False);
  XmToggleButtonSetState(viewer->printOddPages, False, False);
  XmToggleButtonSetState(viewer->printEvenPages, False, False);
  XmToggleButtonSetState(viewer->printBackOrder, False, False);
}

static PSOutputDev *makePSOutputDev(const std::string& psFileName,
                                    PDFDoc *doc,
                                    int firstPage, int lastPage) {
  int imgLLX, imgLLY, imgURX, imgURY;
  xpdfParams->getPSImageableArea(&imgLLX, &imgLLY, &imgURX, &imgURY);

#ifndef GLOBALPARAMS_NO_PSLEVEL
  globalParams->setPSLevel(xpdfParams->getPSLevel());
#endif

#ifdef PSOUTPUTDEV_PAGE_LIST
  std::vector<int> pages;
  for (int i=firstPage; i<=lastPage; i++) {
    pages.push_back(i);
  }
#endif

  return new PSOutputDev(psFileName.c_str(), doc,
        NULL,
#ifdef PSOUTPUTDEV_PAGE_LIST
        pages,
#else
        firstPage, lastPage,
#endif
        psModePS, xpdfParams->getPSPaperWidth(),
        xpdfParams->getPSPaperHeight(),
#ifdef PSOUTPUTDEV_NOCROP
        false,
#endif
        xpdfParams->getPSDuplex(),
        imgLLX, imgLLY, imgURX, imgURY,
#ifdef PSOUTPUTDEV_FORCERASTERIZE
        psRasterizeWhenNeeded,
#else
        false,
#endif
        false, NULL, NULL
#ifdef GLOBALPARAMS_NO_PSLEVEL
        , xpdfParams->getPSLevel()
#endif
        );
}

void XPDFViewer::printPrintCbk(Widget widget, XtPointer ptr,
			       XtPointer callData) {
  XPDFViewer *viewer = (XPDFViewer *)ptr;
  unsigned char withCmd, printAll, printOdd, printEven, printBack;
  std::string psFileName;
  int firstPage, lastPage;
  PDFDoc *doc;
  PSOutputDev *psOut;

  doc = viewer->core->getDoc();
#ifdef ENFORCE_PERMISSIONS
  if (!doc->okToPrint()) {
    error(errNotAllowed, -1, "Printing this document is not allowed.");
    return;
  }
#endif

  viewer->core->setBusyCursor(true);

  XtVaGetValues(viewer->printWithCmdBtn, XmNset, &withCmd, NULL);
  XtVaGetValues(viewer->printAllPages, XmNset, &printAll, NULL);
  XtVaGetValues(viewer->printOddPages, XmNset, &printOdd, NULL);
  XtVaGetValues(viewer->printEvenPages, XmNset, &printEven, NULL);
  XtVaGetValues(viewer->printBackOrder, XmNset, &printBack, NULL);

  if (withCmd) {
    psFileName = std::string(XmTextFieldGetString(viewer->printCmdText));
    psFileName.insert(0, "|");
  } else {
    psFileName = std::string(XmTextFieldGetString(viewer->printFileText));
  }

  firstPage = atoi(XmTextFieldGetString(viewer->printFirstPage));
  lastPage = atoi(XmTextFieldGetString(viewer->printLastPage));
  if (firstPage < 1) {
    firstPage = 1;
  } else if (firstPage > doc->getNumPages()) {
    firstPage = doc->getNumPages();
  }
  if (lastPage < firstPage) {
    lastPage = firstPage;
  } else if (lastPage > doc->getNumPages()) {
    lastPage = doc->getNumPages();
  }

  // Normal print mode
  if (printAll && !printBack)
  {
    psOut = makePSOutputDev(psFileName, doc, firstPage, lastPage);
    if (psOut->isOk()) {
      doc->displayPages(psOut, firstPage, lastPage, 72, 72,
            0, true, xpdfParams->getPSCrop(), false);
    }
    delete psOut;
  }
  // Additional prints mode's
  else
  {
    int step=1, i;
    int beginPage, endPage;

    if (!printAll)
    {
      step=2;
      if (printEven)
      {
        firstPage+=firstPage&0x01?1:0;
        lastPage-=lastPage&0x01?1:0;
      }
      else
      {
        firstPage+=firstPage&0x01?0:1;
        lastPage-=lastPage&0x01?0:1;
      }
    }

    if (printBack)
    {
      step=-step;
      beginPage=lastPage;
      endPage=firstPage;
    }
    else
    {
      beginPage=firstPage;
      endPage=lastPage;
    }

    if (firstPage<=lastPage)
    {
      for (i=beginPage;; i+=step)
      {
        psOut = makePSOutputDev(psFileName, doc, i, i);
        if (psOut->isOk()) {
          doc->displayPages(psOut, i, i, 72, 72,
                0, true, xpdfParams->getPSCrop(), false);
        }
        else
        {
          delete psOut;
          break;
        }
        delete psOut;
        if (i==endPage) break;
      }
    }
  }

  viewer->core->setBusyCursor(false);
}

//------------------------------------------------------------------------
// Motif support
//------------------------------------------------------------------------

XmFontList XPDFViewer::createFontList(char *xlfd) {
  XmFontList fontList;

#if XmVersion <= 1001

  XFontStruct *font;
  String params;
  Cardinal nParams;

  font = XLoadQueryFont(display, xlfd);
  if (font) {
    fontList = XmFontListCreate(font, XmSTRING_DEFAULT_CHARSET);
  } else {
    params = (String)xlfd;
    nParams = 1;
    XtAppWarningMsg(app->getAppContext(),
		    "noSuchFont", "CvtStringToXmFontList",
		    "XtToolkitError", "No such font: %s",
		    &params, &nParams);
    fontList = NULL;
  }

#else

  XmFontListEntry entry;

  entry = XmFontListEntryLoad(display, xlfd,
			      XmFONT_IS_FONT, XmFONTLIST_DEFAULT_TAG);
  fontList = XmFontListAppendEntry(NULL, entry);
  XmFontListEntryFree(&entry);

#endif

  return fontList;
}
