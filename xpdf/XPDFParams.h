//========================================================================
//
// XPDFParams.h
//
// Copyright 2001-2003 Glyph & Cog, LLC
// Copyright 2013 Dmitry Shachnev <mitya57@ubuntu.com>
// Copyright 2014-2020 Adam Sampson <ats@offog.org>
//
// This class is a subset of GlobalParams from the original xpdf, containing
// the settings and config file parser that aren't in Poppler's GlobalParams.
//
//========================================================================

#ifndef XPDFPARAMS_H
#define XPDFPARAMS_H

#include <poppler-config.h>
#include <GlobalParams.h>
#include <memory>
#include <string>
#include <vector>
#include "PSOutputDev.h"

#include <stdio.h>

#if MULTITHREADED
#include <goo/GooMutex.h>
#endif

class XPDFParams;

using StringList = std::vector<std::string>;

//------------------------------------------------------------------------

// The global parameters object.
extern std::unique_ptr<XPDFParams> xpdfParams;

//------------------------------------------------------------------------

class KeyBinding {
public:

  int code;			// 0x20 .. 0xfe = ASCII,
				//   >=0x10000 = special keys, mouse buttons,
				//   etc. (xpdfKeyCode* symbols)
  int mods;			// modifiers (xpdfKeyMod* symbols, or-ed
				//   together)
  int context;			// context (xpdfKeyContext* symbols, or-ed
				//   together)
  StringList cmds;		// list of commands

  KeyBinding(int codeA, int modsA, int contextA, const char *cmd0);
  KeyBinding(int codeA, int modsA, int contextA,
	     const char *cmd0, const char *cmd1);
  KeyBinding(int codeA, int modsA, int contextA, const StringList &cmdsA);
};

#define xpdfKeyCodeTab            0x1000
#define xpdfKeyCodeReturn         0x1001
#define xpdfKeyCodeEnter          0x1002
#define xpdfKeyCodeBackspace      0x1003
#define xpdfKeyCodeInsert         0x1004
#define xpdfKeyCodeDelete         0x1005
#define xpdfKeyCodeHome           0x1006
#define xpdfKeyCodeEnd            0x1007
#define xpdfKeyCodePgUp           0x1008
#define xpdfKeyCodePgDn           0x1009
#define xpdfKeyCodeLeft           0x100a
#define xpdfKeyCodeRight          0x100b
#define xpdfKeyCodeUp             0x100c
#define xpdfKeyCodeDown           0x100d
#define xpdfKeyCodeEscape         0x100e
#define xpdfKeyCodeF1             0x1100
#define xpdfKeyCodeF35            0x1122
#define xpdfKeyCodeMousePress1    0x2001
#define xpdfKeyCodeMousePress2    0x2002
#define xpdfKeyCodeMousePress3    0x2003
#define xpdfKeyCodeMousePress4    0x2004
#define xpdfKeyCodeMousePress5    0x2005
#define xpdfKeyCodeMousePress6    0x2006
#define xpdfKeyCodeMousePress7    0x2007
// ...
#define xpdfKeyCodeMousePress32   0x2020
#define xpdfKeyCodeMouseRelease1  0x2101
#define xpdfKeyCodeMouseRelease2  0x2102
#define xpdfKeyCodeMouseRelease3  0x2103
#define xpdfKeyCodeMouseRelease4  0x2104
#define xpdfKeyCodeMouseRelease5  0x2105
#define xpdfKeyCodeMouseRelease6  0x2106
#define xpdfKeyCodeMouseRelease7  0x2107
// ...
#define xpdfKeyCodeMouseRelease32 0x2120
#define xpdfKeyModNone            0
#define xpdfKeyModShift           (1 << 0)
#define xpdfKeyModCtrl            (1 << 1)
#define xpdfKeyModAlt             (1 << 2)
#define xpdfKeyContextAny         0
#define xpdfKeyContextFullScreen  (1 << 0)
#define xpdfKeyContextWindow      (2 << 0)
#define xpdfKeyContextContinuous  (1 << 2)
#define xpdfKeyContextSinglePage  (2 << 2)
#define xpdfKeyContextOverLink    (1 << 4)
#define xpdfKeyContextOffLink     (2 << 4)
#define xpdfKeyContextOutline     (1 << 6)
#define xpdfKeyContextMainWin     (2 << 6)
#define xpdfKeyContextScrLockOn   (1 << 8)
#define xpdfKeyContextScrLockOff  (2 << 8)

//------------------------------------------------------------------------

// Wrapper type for a parameter that one of the parsing functions can set.
template <class Value>
class Param {
public:

  // Returns true if the parameter was set successfully.
  virtual bool set(Value v) const = 0;
};

//------------------------------------------------------------------------

class XPDFParams {
public:

  // Initialize the global parameters by attempting to read a config
  // file.
  XPDFParams(const char *cfgFileName);

  ~XPDFParams();

  void parseLine(const char *buf, const std::string& fileName, int line);

  //----- accessors

  // XXX: Poppler has getPSFile, but not setPSFile, so we need our own.
  std::string getPSFile();
  int getPSPaperWidth();
  int getPSPaperHeight();
  void getPSImageableArea(int *llx, int *lly, int *urx, int *ury);
  bool getPSDuplex();
  bool getPSCrop();
  PSLevel getPSLevel();
  std::string getInitialZoom();
  bool getContinuousView();
  const std::string& getPageCommand() { return pageCommand; }
  const std::string& getLaunchCommand() { return launchCommand; }
  const std::string& getURLCommand() { return urlCommand; }
  const std::string& getMovieCommand() { return movieCommand; }
  const StringList &getKeyBinding(int code, int mods, int context);

  //----- functions to set parameters

  void setPSFile(const std::string& file);
  bool setPSPaperSize(const std::string& size);
  void setPSPaperWidth(int width);
  void setPSPaperHeight(int height);
  void setPSImageableArea(int llx, int lly, int urx, int ury);
  void setPSDuplex(bool duplex);
  void setPSCrop(bool crop);
  void setPSLevel(PSLevel level);
  void setInitialZoom(const std::string& s);
  void setContinuousView(bool cont);
  void setPageCommand(const std::string& cmd);

private:

  void createDefaultKeyBindings();
  void parseFile(const std::string& fileName, FILE *f);
  void parseToUnicodeDir(const StringList& tokens,
			 const std::string& fileName, int line);
  void parseFontFile(const StringList& tokens,
		     const std::string& fileName, int line);
  void parsePSFile(const StringList& tokens, const std::string& fileName,
		   int line);
  void parsePSPaperSize(const StringList& tokens,
			const std::string& fileName, int line);
  void parsePSImageableArea(const StringList& tokens,
			    const std::string& fileName, int line);
  void parsePSLevel(const StringList& tokens, const std::string& fileName,
		    int line);
  void parseInitialZoom(const StringList& tokens, const std::string& fileName,
			int line);
  void parseBind(const StringList& tokens, const std::string& fileName,
		 int line);
  void parseUnbind(const StringList& tokens, const std::string& fileName,
		   int line);
  bool parseKey(const std::string& modKeyStr, const std::string& contextStr,
		int *code, int *mods, int *context,
		const char *cmdName,
		const StringList& tokens, const std::string& fileName,
		int line);
  void parseCommand(const char *cmdName, std::string& val,
		    const StringList& tokens, const std::string& fileName,
		    int line);
  // Const and non-const versions of this.
  void parseString(const char *cmdName, const Param<char *> &param,
		   const StringList& tokens, const std::string& fileName,
		   int line);
  void parseString(const char *cmdName, const Param<const char *> &param,
		   const StringList& tokens, const std::string& fileName,
		   int line);
  void parseYesNo(const char *cmdName, const Param<bool> &param,
		  const StringList& tokens, const std::string& fileName,
		  int line);
  void parseInteger(const char *cmdName, const Param<int> &param,
		    const StringList& tokens, const std::string& fileName,
		    int line);
  void parseFloat(const char *cmdName, const Param<double> &param,
		  const StringList& tokens, const std::string& fileName,
		  int line);

  //----- user-modifiable settings

  std::string psFile;		// PostScript file or command (for xpdf)
  int psPaperWidth;		// paper size, in PostScript points, for
  int psPaperHeight;		//   PostScript output
  int psImageableLLX,		// imageable area, in PostScript points,
      psImageableLLY,		//   for PostScript output
      psImageableURX,
      psImageableURY;
  bool psCrop;			// crop PS output to CropBox
  bool psDuplex;		// enable duplexing in PostScript?
  PSLevel psLevel;		// PostScript level to generate
  std::string initialZoom;	// initial zoom level
  bool continuousView;		// continuous view mode
  std::string pageCommand;	// command executed on page change
  std::string launchCommand;	// command executed for 'launch' links
  std::string urlCommand;	// command executed for URL links
  std::string movieCommand;	// command executed for movie annotations
  std::vector<KeyBinding> keyBindings;	// key & mouse button bindings

#if MULTITHREADED
  GooMutex mutex;
#endif
};

#endif
