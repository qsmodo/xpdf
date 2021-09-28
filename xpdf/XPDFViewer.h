//========================================================================
//
// XPDFViewer.h
//
// Copyright 2002-2003 Glyph & Cog, LLC
//
//========================================================================

#ifndef XPDFVIEWER_H
#define XPDFVIEWER_H

#include <poppler-config.h>

#include <memory>
#include <string>
#include <vector>

#define Object XtObject
#include <Xm/XmAll.h>
#undef Object
#include "XPDFCore.h"
#include "config.h"

#if (XmVERSION <= 1) && !defined(__sgi)
#define DISABLE_OUTLINE
#endif

#if (XmVERSION >= 2 && !defined(LESSTIF_VERSION))
#  define USE_COMBO_BOX 1
#else
#  undef USE_COMBO_BOX
#endif

class GooString;
class UnicodeMap;
class LinkDest;
class XPDFApp;
class XPDFViewer;

#ifdef NO_GOOLIST
class OutlineItem;
typedef std::vector<OutlineItem *> OutlineItemList;
#define getOILSize size
#else
class GooList;
typedef GooList OutlineItemList;
#define getOILSize getLength
#endif

//------------------------------------------------------------------------

using CmdList = std::vector<std::string>;

struct XPDFViewerCmd {
  const char *name;
  unsigned int nArgs;
  bool requiresDoc;
  bool requiresEvent;
  void (XPDFViewer::*func)(const CmdList& args, XEvent *event);
};

//------------------------------------------------------------------------

//------------------------------------------------------------------------
// XPDFViewer
//------------------------------------------------------------------------

class XPDFViewer {
public:

  XPDFViewer(XPDFApp *appA, const std::string *fileName,
	     int pageA, const std::string *destName, bool fullScreen,
	     const std::string *ownerPassword, const std::string *userPassword);
  XPDFViewer(XPDFApp *appA, PDFDoc *doc, int pageA,
	     const std::string *destName, bool fullScreen);
  bool isOk() { return ok; }
  ~XPDFViewer();

  void open(const std::string& fileName, int pageA,
	    const std::string *destName);
  void clear();
  void reloadFile();

  void execCmd(const std::string& cmd, XEvent *event);

  Widget getWindow() { return win; }

private:

  //----- constructor helper
  void init(XPDFApp *appA, PDFDoc *doc, const std::string *fileName,
	     int pageA, const std::string *destName, bool fullScreen,
	     const std::string *ownerPassword, const std::string *userPassword);

  //----- load / display
  bool loadFile(const std::string& fileName,
		const std::string *ownerPassword = NULL,
		const std::string *userPassword = NULL);
  void displayPage(int pageA, double zoomA, int rotateA,
                   bool scrollToTop, bool addToHist);
  void displayDest(const std::unique_ptr<LinkDest> &dest, double zoomA, int rotateA,
		   bool addToHist);
  void getPageAndDest(int pageA, const std::string *destName,
		      int &pageOut, std::unique_ptr<LinkDest> &destOut);

  //----- hyperlinks / actions
  void doLink(int wx, int wy, bool onlyIfNoSelection, bool newWin);
  static void actionCbk(void *data, const char *action);

  //----- keyboard/mouse input
  static void keyPressCbk(void *data, KeySym key, unsigned int modifiers,
			  XEvent *event);
  static void mouseCbk(void *data, XEvent *event);
  int getModifiers(unsigned int modifiers);
  int getContext(unsigned int modifiers);

  //----- command functions
  void cmdAbout(const CmdList& args, XEvent *event);
  void cmdCloseOutline(const CmdList& args, XEvent *event);
  void cmdCloseWindow(const CmdList& args, XEvent *event);
  void cmdCloseWindowOrQuit(const CmdList& args, XEvent *event);
  void cmdContinuousMode(const CmdList& args, XEvent *event);
  void cmdEndPan(const CmdList& args, XEvent *event);
  void cmdEndSelection(const CmdList& args, XEvent *event);
  void cmdFind(const CmdList& args, XEvent *event);
  void cmdFindNext(const CmdList& args, XEvent *event);
  void cmdFocusToDocWin(const CmdList& args, XEvent *event);
  void cmdFocusToPageNum(const CmdList& args, XEvent *event);
  void cmdFollowLink(const CmdList& args, XEvent *event);
  void cmdFollowLinkInNewWin(const CmdList& args, XEvent *event);
  void cmdFollowLinkInNewWinNoSel(const CmdList& args, XEvent *event);
  void cmdFollowLinkNoSel(const CmdList& args, XEvent *event);
  void cmdFullScreenMode(const CmdList& args, XEvent *event);
  void cmdGoBackward(const CmdList& args, XEvent *event);
  void cmdGoForward(const CmdList& args, XEvent *event);
  void cmdGotoDest(const CmdList& args, XEvent *event);
  void cmdGotoLastPage(const CmdList& args, XEvent *event);
  void cmdGotoLastPageNoScroll(const CmdList& args, XEvent *event);
  void cmdGotoPage(const CmdList& args, XEvent *event);
  void cmdGotoPageNoScroll(const CmdList& args, XEvent *event);
  void cmdNextPage(const CmdList& args, XEvent *event);
  void cmdNextPageNoScroll(const CmdList& args, XEvent *event);
  void cmdOpen(const CmdList& args, XEvent *event);
  void cmdOpenFile(const CmdList& args, XEvent *event);
  void cmdOpenFileAtDest(const CmdList& args, XEvent *event);
  void cmdOpenFileAtDestInNewWin(const CmdList& args, XEvent *event);
  void cmdOpenFileAtPage(const CmdList& args, XEvent *event);
  void cmdOpenFileAtPageInNewWin(const CmdList& args, XEvent *event);
  void cmdOpenFileInNewWin(const CmdList& args, XEvent *event);
  void cmdOpenInNewWin(const CmdList& args, XEvent *event);
  void cmdOpenOutline(const CmdList& args, XEvent *event);
  void cmdPageDown(const CmdList& args, XEvent *event);
  void cmdPageUp(const CmdList& args, XEvent *event);
  void cmdPostPopupMenu(const CmdList& args, XEvent *event);
  void cmdPrevPage(const CmdList& args, XEvent *event);
  void cmdPrevPageNoScroll(const CmdList& args, XEvent *event);
  void cmdPrint(const CmdList& args, XEvent *event);
  void cmdQuit(const CmdList& args, XEvent *event);
  void cmdRaise(const CmdList& args, XEvent *event);
  void cmdRedraw(const CmdList& args, XEvent *event);
  void cmdReload(const CmdList& args, XEvent *event);
  void cmdRotateCCW(const CmdList& args, XEvent *event);
  void cmdRotateCW(const CmdList& args, XEvent *event);
  void cmdRun(const CmdList& args, XEvent *event);
  void cmdSaveAs(const CmdList& args, XEvent *event);
  void cmdScrollDown(const CmdList& args, XEvent *event);
  void cmdScrollDownNextPage(const CmdList& args, XEvent *event);
  void cmdScrollLeft(const CmdList& args, XEvent *event);
  void cmdScrollOutlineDown(const CmdList& args, XEvent *event);
  void cmdScrollOutlineUp(const CmdList& args, XEvent *event);
  void cmdScrollRight(const CmdList& args, XEvent *event);
  void cmdScrollToBottomEdge(const CmdList& args, XEvent *event);
  void cmdScrollToBottomRight(const CmdList& args, XEvent *event);
  void cmdScrollToLeftEdge(const CmdList& args, XEvent *event);
  void cmdScrollToRightEdge(const CmdList& args, XEvent *event);
  void cmdScrollToTopEdge(const CmdList& args, XEvent *event);
  void cmdScrollToTopLeft(const CmdList& args, XEvent *event);
  void cmdScrollUp(const CmdList& args, XEvent *event);
  void cmdScrollUpPrevPage(const CmdList& args, XEvent *event);
  void cmdSearch(const CmdList& args, XEvent *event);
  void cmdSetSelection(const CmdList& args, XEvent *event);
  void cmdSinglePageMode(const CmdList& args, XEvent *event);
  void cmdStartPan(const CmdList& args, XEvent *event);
  void cmdStartSelection(const CmdList& args, XEvent *event);
  void cmdToggleContinuousMode(const CmdList& args, XEvent *event);
  void cmdToggleFullScreenMode(const CmdList& args, XEvent *event);
  void cmdToggleOutline(const CmdList& args, XEvent *event);
  void cmdWindowMode(const CmdList& args, XEvent *event);
  void cmdZoomFitPage(const CmdList& args, XEvent *event);
  void cmdZoomFitWidth(const CmdList& args, XEvent *event);
  void cmdZoomFitHeight(const CmdList& args, XEvent *event);
  void cmdZoomIn(const CmdList& args, XEvent *event);
  void cmdZoomOut(const CmdList& args, XEvent *event);
  void cmdZoomPercent(const CmdList& args, XEvent *event);
  void cmdZoomToSelection(const CmdList& args, XEvent *event);

  //----- GUI code: main window
  void initWindow(bool fullScreen);
  void initToolbar(Widget parent);
#ifndef DISABLE_OUTLINE
  void initPanedWin(Widget parent);
#endif
  void initCore(Widget parent, bool fullScreen);
  void initPopupMenu();
  void addToolTip(Widget widget, char *text);
  void mapWindow();
  void closeWindow();
  int getZoomIdx();
  void setZoomIdx(int idx);
  void setZoomVal(double z);
  static void prevPageCbk(Widget widget, XtPointer ptr,
			  XtPointer callData);
  static void prevTenPageCbk(Widget widget, XtPointer ptr,
			     XtPointer callData);
  static void nextPageCbk(Widget widget, XtPointer ptr,
			  XtPointer callData);
  static void nextTenPageCbk(Widget widget, XtPointer ptr,
			     XtPointer callData);
  static void backCbk(Widget widget, XtPointer ptr,
		      XtPointer callData);
  static void forwardCbk(Widget widget, XtPointer ptr,
			 XtPointer callData);
#if USE_COMBO_BOX
  static void zoomComboBoxCbk(Widget widget, XtPointer ptr,
			      XtPointer callData);
#else
  static void zoomMenuCbk(Widget widget, XtPointer ptr,
			  XtPointer callData);
#endif
  static void findCbk(Widget widget, XtPointer ptr,
		      XtPointer callData);
  static void printCbk(Widget widget, XtPointer ptr,
		       XtPointer callData);
  static void aboutCbk(Widget widget, XtPointer ptr,
		       XtPointer callData);
  static void quitCbk(Widget widget, XtPointer ptr,
		      XtPointer callData);
  static void openCbk(Widget widget, XtPointer ptr,
		      XtPointer callData);
  static void openInNewWindowCbk(Widget widget, XtPointer ptr,
				 XtPointer callData);
  static void reloadCbk(Widget widget, XtPointer ptr,
			XtPointer callData);
  static void saveAsCbk(Widget widget, XtPointer ptr,
			XtPointer callData);
  static void continuousModeToggleCbk(Widget widget, XtPointer ptr,
				      XtPointer callData);
  static void fullScreenToggleCbk(Widget widget, XtPointer ptr,
				  XtPointer callData);
  static void rotateCCWCbk(Widget widget, XtPointer ptr,
			   XtPointer callData);
  static void rotateCWCbk(Widget widget, XtPointer ptr,
			  XtPointer callData);
  static void zoomToSelectionCbk(Widget widget, XtPointer ptr,
				 XtPointer callData);
  static void closeCbk(Widget widget, XtPointer ptr,
		       XtPointer callData);
  static void closeMsgCbk(Widget widget, XtPointer ptr,
			  XtPointer callData);
  static void pageNumCbk(Widget widget, XtPointer ptr,
			 XtPointer callData);
  static void updateCbk(void *data, PCONST GooString *fileName,
			int pageNum, int numPages, const char *linkString);

  //----- GUI code: outline
#ifndef DISABLE_OUTLINE
  void setupOutline();
  void setupOutlineItems(PCONST OutlineItemList *items, Widget parent);
  static void outlineSelectCbk(Widget widget, XtPointer ptr,
			       XtPointer callData);
#endif

  //----- GUI code: "about" dialog
  void initAboutDialog();

  //----- GUI code: "open" dialog
  void initOpenDialog();
  void mapOpenDialog(bool openInNewWindowA);
  static void openOkCbk(Widget widget, XtPointer ptr,
			XtPointer callData);

  //----- GUI code: "find" dialog
  void initFindDialog();
  static void findFindCbk(Widget widget, XtPointer ptr,
			  XtPointer callData);
  void mapFindDialog();
  void doFind(bool next);
  static void findCloseCbk(Widget widget, XtPointer ptr,
			   XtPointer callData);

  //----- GUI code: "save as" dialog
  void initSaveAsDialog();
  void mapSaveAsDialog();
  static void saveAsOkCbk(Widget widget, XtPointer ptr,
			  XtPointer callData);

  //----- GUI code: "print" dialog
  void initPrintDialog();
  void setupPrintDialog();
  static void printWithCmdBtnCbk(Widget widget, XtPointer ptr,
				 XtPointer callData);
  static void printToFileBtnCbk(Widget widget, XtPointer ptr,
				XtPointer callData);

  static void printAllPagesBtnCbk(Widget widget, XtPointer ptr,
				 XtPointer callData);
  static void printEvenPagesBtnCbk(Widget widget, XtPointer ptr,
				 XtPointer callData);
  static void printOddPagesBtnCbk(Widget widget, XtPointer ptr,
				 XtPointer callData);
  static void printBackOrderBtnCbk(Widget widget, XtPointer ptr,
				 XtPointer callData);

  static void printPrintCbk(Widget widget, XtPointer ptr,
			    XtPointer callData);

  //----- Motif support
  XmFontList createFontList(char *xlfd);

  static XPDFViewerCmd cmdTab[];

  XPDFApp *app;
  bool ok;

  Display *display;
  int screenNum;
  Widget win;			// top-level window
  Widget form;
  Widget panedWin;
#ifndef DISABLE_OUTLINE
  Widget outlineScroll;
  Widget outlineTree;
  Widget *outlineLabels;
  int outlineLabelsLength;
  int outlineLabelsSize;
  Dimension outlinePaneWidth;
#endif
  XPDFCore *core;
  Widget toolBar;
  Widget backBtn;
  Widget prevTenPageBtn;
  Widget prevPageBtn;
  Widget nextPageBtn;
  Widget nextTenPageBtn;
  Widget forwardBtn;
  Widget pageNumText;
  Widget pageCountLabel;
#if USE_COMBO_BOX
  Widget zoomComboBox;
#else
  Widget zoomMenu;
#endif
  Widget zoomWidget;
  Widget findBtn;
  Widget printBtn;
  Widget aboutBtn;
  Widget linkLabel;
  Widget quitBtn;
  Widget popupMenu;

  Widget aboutDialog;
  XmFontList aboutBigFont, aboutVersionFont, aboutFixedFont;

  Widget openDialog;
  bool openInNewWindow;

  Widget findDialog;
  Widget findText;
  Widget findBackwardToggle;
  Widget findCaseSensitiveToggle;
  Widget findWholeWordToggle;

  Widget saveAsDialog;

  Widget printDialog;
  Widget printWithCmdBtn;
  Widget printToFileBtn;
  Widget printCmdText;
  Widget printFileText;
  Widget printFirstPage;
  Widget printLastPage;

  Widget printAllPages, printEvenPages, printOddPages, printBackOrder;
};

#endif
