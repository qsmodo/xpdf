//========================================================================
//
// XPDFCore.h
//
// Copyright 2002-2003 Glyph & Cog, LLC
//
//========================================================================

#ifndef XPDFCORE_H
#define XPDFCORE_H

#include <poppler-config.h>

#include <memory>
#include <string>
#define Object XtObject
#include <Xm/XmAll.h>
#undef Object
#include <goo/gfile.h> // for time_t
#include <goo/GooString.h>
#include <splash/SplashTypes.h>
#include "PDFCore.h"
#include "config.h"

class BaseStream;
class PDFDoc;
class LinkAction;

//------------------------------------------------------------------------

#define xMaxRGBCube 6		// max size of RGB color cube

//------------------------------------------------------------------------
// callbacks
//------------------------------------------------------------------------

typedef void (*XPDFUpdateCbk)(void *data, PCONST GooString *fileName,
			      int pageNum, int numPages,
			      const char *linkLabel);

typedef void (*XPDFActionCbk)(void *data, const char *action);

typedef void (*XPDFKeyPressCbk)(void *data, KeySym key, unsigned int modifiers,
				XEvent *event);

typedef void (*XPDFMouseCbk)(void *data, XEvent *event);

//------------------------------------------------------------------------
// XPDFCore
//------------------------------------------------------------------------

class XPDFCore: public PDFCore {
public:

  // Create viewer core inside <parentWidgetA>.
  XPDFCore(Widget shellA, Widget parentWidgetA,
	   SplashColorPtr paperColorA, unsigned long paperPixelA,
	   unsigned long mattePixelA, bool fullScreenA, bool reverseVideoA,
	   bool installCmap, int rgbCubeSizeA);

  ~XPDFCore();

  //----- loadFile / displayPage / displayDest

  // Load a new file.  Returns pdfOk or error code.
  virtual int loadFile(const std::string& fileName,
		       const std::string *ownerPassword = NULL,
		       const std::string *userPassword = NULL);

  // Load a new file, via a Stream instead of a file name.  Returns
  // pdfOk or error code.
  virtual int loadFile(BaseStream *stream,
		       const std::string *ownerPassword = NULL,
		       const std::string *userPassword = NULL);

  // Load an already-created PDFDoc object.
  virtual void loadDoc(PDFDoc *docA);

  // Resize the window to fit page <pg> of the current document.
  void resizeToPage(int pg);

  // Update the display, given the specified parameters.
  virtual void update(int topPageA, int scrollXA, int scrollYA,
		      double zoomA, int rotateA, bool force,
		      bool addToHist, bool adjustScrollX);

  //----- page/position changes

  virtual bool gotoNextPage(int inc, bool top);
  virtual bool gotoPrevPage(int dec, bool top, bool bottom);
  virtual bool goForward();
  virtual bool goBackward();
  void startPan(int wx, int wy);
  void endPan(int wx, int wy);

  //----- selection

  void startSelection(int wx, int wy);
  void endSelection(int wx, int wy);
  void copySelection();

  //----- hyperlinks

  void doAction(PCONST LinkAction *action);
  LinkAction *getLinkAction() { return linkAction; }
  std::string mungeURL(const std::string &url);

  //----- find

  virtual bool find(const char *s, bool caseSensitive, bool next,
		    bool backward, bool wholeWord, bool onePageOnly);
  virtual bool findU(Unicode *u, int len, bool caseSensitive,
		     bool next, bool backward,
		     bool wholeWord, bool onePageOnly);

  //----- simple modal dialogs

  bool doQuestionDialog(const char *title, GooString *msg);
  void doInfoDialog(const char *title, GooString *msg);
  void doErrorDialog(const char *title, GooString *msg);

  //----- password dialog

  virtual std::unique_ptr<std::string> getPassword();

  //----- misc access

  Widget getWidget() { return scrolledWin; }
  Widget getDrawAreaWidget() { return drawArea; }
  virtual void setBusyCursor(bool busy);
  Cursor getBusyCursor() { return busyCursor; }
  void takeFocus();
  void enableHyperlinks(bool on) { hyperlinksEnabled = on; }
  bool getHyperlinksEnabled() { return hyperlinksEnabled; }
  void enableSelect(bool on) { selectEnabled = on; }
  void setUpdateCbk(XPDFUpdateCbk cbk, void *data)
    { updateCbk = cbk; updateCbkData = data; }
  void setActionCbk(XPDFActionCbk cbk, void *data)
    { actionCbk = cbk; actionCbkData = data; }
  void setKeyPressCbk(XPDFKeyPressCbk cbk, void *data)
    { keyPressCbk = cbk; keyPressCbkData = data; }
  void setMouseCbk(XPDFMouseCbk cbk, void *data)
    { mouseCbk = cbk; mouseCbkData = data; }
  bool getFullScreen() { return fullScreen; }

private:

  virtual bool checkForNewFile();

  //----- hyperlinks
  std::string getLinkAbsolutePath(const std::string& relPath);
  void runCommand(const std::string& cmdFmt, const std::string& arg);

  //----- selection
  static Boolean convertSelectionCbk(Widget widget, Atom *selection,
				     Atom *target, Atom *type,
				     XtPointer *value, unsigned long *length,
				     int *format);

  //----- GUI code
  void setupX(bool installCmap, int rgbCubeSizeA);
  void initWindow();
  static void hScrollChangeCbk(Widget widget, XtPointer ptr,
			       XtPointer callData);
  static void hScrollDragCbk(Widget widget, XtPointer ptr,
			     XtPointer callData);
  static void vScrollChangeCbk(Widget widget, XtPointer ptr,
			       XtPointer callData);
  static void vScrollDragCbk(Widget widget, XtPointer ptr,
			     XtPointer callData);
  static void resizeCbk(Widget widget, XtPointer ptr, XtPointer callData);
  static void redrawCbk(Widget widget, XtPointer ptr, XtPointer callData);
  static void inputCbk(Widget widget, XtPointer ptr, XtPointer callData);
  virtual PDFCoreTile *newTile(int xDestA, int yDestA);
  virtual void updateTileData(PDFCoreTile *tileA, int xSrc, int ySrc,
			      int width, int height, bool composited);
  virtual void redrawRect(PDFCoreTile *tileA, int xSrc, int ySrc,
			  int xDest, int yDest, int width, int height,
			  bool composited);
  virtual void updateScrollbars();
  void setCursor(Cursor cursor);
  bool doDialog(int type, bool hasCancel,
		const char *title, GooString *msg);
  static void dialogOkCbk(Widget widget, XtPointer ptr,
			  XtPointer callData);
  static void dialogCancelCbk(Widget widget, XtPointer ptr,
			      XtPointer callData);
  void initPasswordDialog();
  static void passwordTextVerifyCbk(Widget widget, XtPointer ptr,
				    XtPointer callData);
  static void passwordOkCbk(Widget widget, XtPointer ptr,
			    XtPointer callData);
  static void passwordCancelCbk(Widget widget, XtPointer ptr,
				XtPointer callData);

  unsigned long paperPixel;
  unsigned long mattePixel;
  //~unimp: move fullScreen into PDFCore?
  bool fullScreen;

  Display *display;
  int screenNum;
  Visual *visual;
  Colormap colormap;
  unsigned int depth;           // visual depth
  bool trueColor;               // set if using a TrueColor visual
  int rDiv, gDiv, bDiv;         // RGB right shifts (for TrueColor)
  int rShift, gShift, bShift;   // RGB left shifts (for TrueColor)
  int rgbCubeSize;              // size of color cube (for non-TrueColor)
  unsigned long                 // color cube (for non-TrueColor)
    colors[xMaxRGBCube * xMaxRGBCube * xMaxRGBCube];

  Widget shell;			// top-level shell containing the widget
  Widget parentWidget;		// parent widget (not created by XPDFCore)
  Widget scrolledWin;
  Widget hScrollBar;
  Widget vScrollBar;
  Widget drawAreaFrame;
  Widget drawArea;
  Cursor busyCursor, linkCursor, selectCursor;
  Cursor currentCursor;
  GC drawAreaGC;		// GC for blitting into drawArea

  static GooString *currentSelection;  // selected text
  static XPDFCore *currentSelectionOwner;
  static Atom targetsAtom;
  static Atom textAtom;
  static Atom compoundtextAtom;
  static Atom utf8stringAtom;

  bool panning;
  int panMX, panMY;

  time_t modTime;		// last modification time of PDF file

  LinkAction *linkAction;	// mouse cursor is over this link

  XPDFUpdateCbk updateCbk;
  void *updateCbkData;
  XPDFActionCbk actionCbk;
  void *actionCbkData;
  XPDFKeyPressCbk keyPressCbk;
  void *keyPressCbkData;
  XPDFMouseCbk mouseCbk;
  void *mouseCbkData;

  bool hyperlinksEnabled;
  bool selectEnabled;

  int dialogDone;

  Widget passwordDialog;
  Widget passwordText;
  std::unique_ptr<std::string> password;
};

#endif
