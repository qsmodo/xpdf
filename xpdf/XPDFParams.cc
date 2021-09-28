//========================================================================
//
// XPDFParams.cc
//
// Copyright 2001-2003 Glyph & Cog, LLC
// Copyright 2013 Dmitry Shachnev <mitya57@ubuntu.com>
// Copyright 2014-2020 Adam Sampson <ats@offog.org>
//
//========================================================================

#include <poppler-config.h>
#include "config.h"

#include <string.h>
#include <stdio.h>
#include <ctype.h>
#if HAVE_PAPER_H
#include <paper.h>
#endif
#include <goo/gmem.h>
#include <goo/GooString.h>
#include <goo/gfile.h>
#include "gfile-xpdf.h"
#include "Error.h"
#include "XPDFParams.h"

#if MULTITHREADED
#  define lockXPDFParams            gLockMutex(&mutex)
#  define unlockXPDFParams          gUnlockMutex(&mutex)
#else
#  define lockXPDFParams
#  define unlockXPDFParams
#endif

//------------------------------------------------------------------------

std::unique_ptr<XPDFParams> xpdfParams;

//------------------------------------------------------------------------
// KeyBinding
//------------------------------------------------------------------------

KeyBinding::KeyBinding(int codeA, int modsA, int contextA, const char *cmd0) {
  code = codeA;
  mods = modsA;
  context = contextA;
  cmds.push_back(cmd0);
}

KeyBinding::KeyBinding(int codeA, int modsA, int contextA,
                       const char *cmd0, const char *cmd1) {
  code = codeA;
  mods = modsA;
  context = contextA;
  cmds.push_back(cmd0);
  cmds.push_back(cmd1);
}

KeyBinding::KeyBinding(int codeA, int modsA, int contextA,
                       const StringList& cmdsA) {
  code = codeA;
  mods = modsA;
  context = contextA;
  cmds = cmdsA;
}

//------------------------------------------------------------------------
// parameter-setting helpers
//------------------------------------------------------------------------

// These classes wrap a mutator method in GlobalParams or XPDFParams to match
// the Param interface, so the parse* methods can write their result into
// either Poppler's settings or xpdf's.

// For setters that take a value and return void.
template <class Params, class ParamsPtr, class Value>
class ParamSetter : public Param<Value> {
public:
  typedef void (Params::*SetFunc)(Value);

  ParamSetter(const ParamsPtr &paramsA, SetFunc funcA)
    : params(paramsA), func(funcA) {}

  bool set(Value v) const {
    ((*params).*func)(v);
    return true;
  }

private:
  const ParamsPtr &params;
  SetFunc func;
};

template <class Params, class ParamsPtr, class Value>
ParamSetter<Params, ParamsPtr, Value>
makeSetter(const ParamsPtr &p, void (Params::*func)(Value)) {
  return ParamSetter<Params, ParamsPtr, Value>(p, func);
}

#define globalParam(mem) makeSetter(globalParams, &GlobalParams::mem)
#define xpdfParam(mem)   makeSetter(this, &XPDFParams::mem)

// For setters that take a value and return a boolean indicating success.
template <class Params, class ParamsPtr, class Value>
class ParamBoolSetter : public Param<Value> {
public:
  typedef bool (Params::*SetFunc)(Value);

  ParamBoolSetter(const ParamsPtr &paramsA, SetFunc funcA)
    : params(paramsA), func(funcA) {}

  bool set(Value v) const {
    return ((*params).*func)(v);
  }

private:
  const ParamsPtr &params;
  SetFunc func;
};

template <class Params, class ParamsPtr, class Value>
ParamBoolSetter<Params, ParamsPtr, Value>
makeBoolSetter(const ParamsPtr &p, bool (Params::*func)(Value)) {
  return ParamBoolSetter<Params, ParamsPtr, Value>(p, func);
}

#define globalString(mem) makeBoolSetter(globalParams, &GlobalParams::mem)
#define xpdfString(mem)   makeBoolSetter(this, &XPDFParams::mem)

//------------------------------------------------------------------------
// parsing
//------------------------------------------------------------------------

XPDFParams::XPDFParams(const char *cfgFileName) {
#if MULTITHREADED
  gInitMutex(&mutex);
#endif

#if HAVE_PAPER_H
  char *paperName;
  const struct paper *paperType;
  paperinit();
  if ((paperName = systempapername())) {
    paperType = paperinfo(paperName);
    psPaperWidth = (int)paperpswidth(paperType);
    psPaperHeight = (int)paperpsheight(paperType);
  } else {
    error(errConfig, -1, "No paper information available - using defaults");
    psPaperWidth = defPaperWidth;
    psPaperHeight = defPaperHeight;
  }
  paperdone();
#else
  psPaperWidth = defPaperWidth;
  psPaperHeight = defPaperHeight;
#endif
  psImageableLLX = psImageableLLY = 0;
  psImageableURX = psPaperWidth;
  psImageableURY = psPaperHeight;
  psCrop = true;
  psDuplex = false;
  initialZoom = "125";
  continuousView = false;
  createDefaultKeyBindings();

  // look for a user config file, then a system-wide config file
  FILE *f = NULL;
  std::string fileName;
  if (cfgFileName && cfgFileName[0]) {
    fileName = cfgFileName;
    f = fopen(fileName.c_str(), "r");
  }
  if (!f) {
    GooString *s = appendToPath(getHomeDir(), xpdfUserConfigFile);
    fileName = s->getCString();
    delete s;
    f = fopen(fileName.c_str(), "r");
  }
  if (!f) {
    fileName = xpdfSysConfigFile;
    f = fopen(fileName.c_str(), "r");
  }
  if (f) {
    parseFile(fileName, f);
    fclose(f);
  }
}

void XPDFParams::createDefaultKeyBindings() {
  //----- mouse buttons
  keyBindings.emplace_back(xpdfKeyCodeMousePress1, xpdfKeyModNone,
                           xpdfKeyContextAny, "startSelection");
  keyBindings.emplace_back(xpdfKeyCodeMouseRelease1, xpdfKeyModNone,
                           xpdfKeyContextAny,
                           "endSelection", "followLinkNoSel");
  keyBindings.emplace_back(xpdfKeyCodeMousePress2, xpdfKeyModNone,
                           xpdfKeyContextAny, "startPan");
  keyBindings.emplace_back(xpdfKeyCodeMouseRelease2, xpdfKeyModNone,
                           xpdfKeyContextAny, "endPan");
  keyBindings.emplace_back(xpdfKeyCodeMousePress3, xpdfKeyModNone,
                           xpdfKeyContextAny, "postPopupMenu");
  keyBindings.emplace_back(xpdfKeyCodeMousePress4, xpdfKeyModNone,
                           xpdfKeyContextAny, "scrollUpPrevPage(16)");
  keyBindings.emplace_back(xpdfKeyCodeMousePress5, xpdfKeyModNone,
                           xpdfKeyContextAny, "scrollDownNextPage(16)");
  keyBindings.emplace_back(xpdfKeyCodeMousePress6, xpdfKeyModNone,
                           xpdfKeyContextAny, "scrollLeft(16)");
  keyBindings.emplace_back(xpdfKeyCodeMousePress7, xpdfKeyModNone,
                           xpdfKeyContextAny, "scrollRight(16)");

  //----- keys
  keyBindings.emplace_back(xpdfKeyCodeHome, xpdfKeyModCtrl,
                           xpdfKeyContextAny, "gotoPage(1)");
  keyBindings.emplace_back(xpdfKeyCodeHome, xpdfKeyModNone,
                           xpdfKeyContextAny, "scrollToTopLeft");
  keyBindings.emplace_back(xpdfKeyCodeEnd, xpdfKeyModCtrl,
                           xpdfKeyContextAny, "gotoLastPage");
  keyBindings.emplace_back(xpdfKeyCodeEnd, xpdfKeyModNone,
                           xpdfKeyContextAny, "scrollToBottomRight");
  keyBindings.emplace_back(xpdfKeyCodePgUp, xpdfKeyModNone,
                           xpdfKeyContextAny, "pageUp");
  keyBindings.emplace_back(xpdfKeyCodeBackspace, xpdfKeyModNone,
                           xpdfKeyContextAny, "pageUp");
  keyBindings.emplace_back(xpdfKeyCodeDelete, xpdfKeyModNone,
                           xpdfKeyContextAny, "pageUp");
  keyBindings.emplace_back(xpdfKeyCodePgDn, xpdfKeyModNone,
                           xpdfKeyContextAny, "pageDown");
  keyBindings.emplace_back(' ', xpdfKeyModNone,
                           xpdfKeyContextAny, "pageDown");
  keyBindings.emplace_back(xpdfKeyCodeLeft, xpdfKeyModNone,
                           xpdfKeyContextAny, "scrollLeft(16)");
  keyBindings.emplace_back(xpdfKeyCodeRight, xpdfKeyModNone,
                           xpdfKeyContextAny, "scrollRight(16)");
  keyBindings.emplace_back(xpdfKeyCodeUp, xpdfKeyModNone,
                           xpdfKeyContextAny, "scrollUp(16)");
  keyBindings.emplace_back(xpdfKeyCodeDown, xpdfKeyModNone,
                           xpdfKeyContextAny, "scrollDown(16)");
  keyBindings.emplace_back('o', xpdfKeyModNone,
                           xpdfKeyContextAny, "open");
  keyBindings.emplace_back('O', xpdfKeyModNone,
                           xpdfKeyContextAny, "open");
  keyBindings.emplace_back('r', xpdfKeyModNone,
                           xpdfKeyContextAny, "reload");
  keyBindings.emplace_back('R', xpdfKeyModNone,
                           xpdfKeyContextAny, "reload");
  keyBindings.emplace_back('s', xpdfKeyModNone,
                           xpdfKeyContextAny, "saveAs");
  keyBindings.emplace_back('S', xpdfKeyModNone,
                           xpdfKeyContextAny, "saveAs");
  keyBindings.emplace_back('f', xpdfKeyModNone,
                           xpdfKeyContextAny, "find");
  keyBindings.emplace_back('F', xpdfKeyModNone,
                           xpdfKeyContextAny, "find");
  keyBindings.emplace_back('f', xpdfKeyModCtrl,
                           xpdfKeyContextAny, "find");
  keyBindings.emplace_back('/', xpdfKeyModNone,
                           xpdfKeyContextAny, "find");
  keyBindings.emplace_back('g', xpdfKeyModCtrl,
                           xpdfKeyContextAny, "findNext");
  keyBindings.emplace_back('p', xpdfKeyModCtrl,
                           xpdfKeyContextAny, "print");
  keyBindings.emplace_back('n', xpdfKeyModNone,
                           xpdfKeyContextScrLockOff, "nextPage");
  keyBindings.emplace_back('N', xpdfKeyModNone,
                           xpdfKeyContextScrLockOff, "nextPage");
  keyBindings.emplace_back('n', xpdfKeyModNone,
                           xpdfKeyContextScrLockOn, "nextPageNoScroll");
  keyBindings.emplace_back('N', xpdfKeyModNone,
                           xpdfKeyContextScrLockOn, "nextPageNoScroll");
  keyBindings.emplace_back('p', xpdfKeyModNone,
                           xpdfKeyContextScrLockOff, "prevPage");
  keyBindings.emplace_back('P', xpdfKeyModNone,
                           xpdfKeyContextScrLockOff, "prevPage");
  keyBindings.emplace_back('p', xpdfKeyModNone,
                           xpdfKeyContextScrLockOn, "prevPageNoScroll");
  keyBindings.emplace_back('P', xpdfKeyModNone,
                           xpdfKeyContextScrLockOn, "prevPageNoScroll");
  keyBindings.emplace_back('[', xpdfKeyModNone,
                           xpdfKeyContextAny, "rotateCCW");
  keyBindings.emplace_back(']', xpdfKeyModNone,
                           xpdfKeyContextAny, "rotateCW");
  keyBindings.emplace_back('v', xpdfKeyModNone,
                           xpdfKeyContextAny, "goForward");
  keyBindings.emplace_back('V', xpdfKeyModNone,
                           xpdfKeyContextAny, "goForward");
  keyBindings.emplace_back('b', xpdfKeyModNone,
                           xpdfKeyContextAny, "goBackward");
  keyBindings.emplace_back('B', xpdfKeyModNone,
                           xpdfKeyContextAny, "goBackward");
  keyBindings.emplace_back('g', xpdfKeyModNone,
                           xpdfKeyContextAny, "focusToPageNum");
  keyBindings.emplace_back('G', xpdfKeyModNone,
                           xpdfKeyContextAny, "focusToPageNum");
  keyBindings.emplace_back('0', xpdfKeyModNone,
                           xpdfKeyContextAny, "zoomPercent(125)");
  keyBindings.emplace_back('+', xpdfKeyModNone,
                           xpdfKeyContextAny, "zoomIn");
  keyBindings.emplace_back('-', xpdfKeyModNone,
                           xpdfKeyContextAny, "zoomOut");
  keyBindings.emplace_back('z', xpdfKeyModNone,
                           xpdfKeyContextAny, "zoomFitPage");
  keyBindings.emplace_back('Z', xpdfKeyModNone,
                           xpdfKeyContextAny, "zoomFitPage");
  keyBindings.emplace_back('h', xpdfKeyModNone,
                           xpdfKeyContextAny, "zoomFitHeight");
  keyBindings.emplace_back('H', xpdfKeyModNone,
                           xpdfKeyContextAny, "zoomFitHeight");
  keyBindings.emplace_back('w', xpdfKeyModNone,
                           xpdfKeyContextAny, "zoomFitWidth");
  keyBindings.emplace_back('W', xpdfKeyModNone,
                           xpdfKeyContextAny, "zoomFitWidth");
  keyBindings.emplace_back('f', xpdfKeyModAlt,
                           xpdfKeyContextAny, "toggleFullScreenMode");
  keyBindings.emplace_back('l', xpdfKeyModCtrl,
                           xpdfKeyContextAny, "redraw");
  keyBindings.emplace_back('w', xpdfKeyModCtrl,
                           xpdfKeyContextAny, "closeWindowOrQuit");
  keyBindings.emplace_back('?', xpdfKeyModNone,
                           xpdfKeyContextAny, "about");
  keyBindings.emplace_back('q', xpdfKeyModNone,
                           xpdfKeyContextAny, "quit");
  keyBindings.emplace_back('Q', xpdfKeyModNone,
                           xpdfKeyContextAny, "quit");
  keyBindings.emplace_back(xpdfKeyCodeEscape, xpdfKeyModNone,
                           xpdfKeyContextAny, "quit");
}

void XPDFParams::parseFile(const std::string& fileName, FILE *f) {
  int line;
  char buf[512];

  line = 1;
  while (getLine(buf, sizeof(buf) - 1, f)) {
    parseLine(buf, fileName, line);
    ++line;
  }
}

void XPDFParams::parseLine(const char *buf, const std::string& fileName,
                           int line) {
  StringList tokens;
  const char *p1, *p2;
  FILE *f2;

  // break the line into tokens
  p1 = buf;
  while (*p1) {
    for (; *p1 && isspace(*p1); ++p1) ;
    if (!*p1) {
      break;
    }
    if (*p1 == '"' || *p1 == '\'') {
      for (p2 = p1 + 1; *p2 && *p2 != *p1; ++p2) ;
      ++p1;
    } else {
      for (p2 = p1 + 1; *p2 && !isspace(*p2); ++p2) ;
    }
    tokens.emplace_back(p1, (int)(p2 - p1));
    p1 = *p2 ? p2 + 1 : p2;
  }

  // parse the line
  if (!tokens.empty() && tokens[0][0] != '#') {
    const auto& cmd = tokens[0];
    if (cmd == "include") {
      if (tokens.size() == 2) {
        const auto& incFile = tokens[1];
        if ((f2 = openFile(incFile.c_str(), "r"))) {
          parseFile(incFile, f2);
          fclose(f2);
        } else {
          error(errConfig, -1,
                "Couldn't find included config file: '{0:s}' ({1:s}:{2:d})",
                incFile.c_str(), fileName.c_str(), line);
        }
      } else {
        error(errConfig, -1, "Bad 'include' config file command ({0:s}:{1:d})",
              fileName.c_str(), line);
      }
    } else if (cmd == "fontFile") {
      parseFontFile(tokens, fileName, line);
    } else if (cmd == "psFile") {
      parsePSFile(tokens, fileName, line);
    } else if (cmd == "psPaperSize") {
      parsePSPaperSize(tokens, fileName, line);
    } else if (cmd == "psImageableArea") {
      parsePSImageableArea(tokens, fileName, line);
    } else if (cmd == "psCrop") {
      parseYesNo("psCrop", xpdfParam(setPSCrop), tokens, fileName, line);
    } else if (cmd == "psExpandSmaller") {
      parseYesNo("psExpandSmaller", globalParam(setPSExpandSmaller),
                 tokens, fileName, line);
    } else if (cmd == "psShrinkLarger") {
      parseYesNo("psShrinkLarger", globalParam(setPSShrinkLarger),
                 tokens, fileName, line);
    } else if (cmd == "psDuplex") {
      parseYesNo("psDuplex", xpdfParam(setPSDuplex), tokens, fileName, line);
    } else if (cmd == "psLevel") {
      parsePSLevel(tokens, fileName, line);
    } else if (cmd == "textEncoding") {
      parseString("textEncoding", globalParam(setTextEncoding),
                  tokens, fileName, line);
    } else if (cmd == "initialZoom") {
      parseInitialZoom(tokens, fileName, line);
    } else if (cmd == "continuousView") {
      parseYesNo("continuousView", xpdfParam(setContinuousView),
                 tokens, fileName, line);
    } else if (cmd == "overprintPreview") {
      parseYesNo("overprintPreview", globalParam(setOverprintPreview),
                 tokens, fileName, line);
    } else if (cmd == "pageCommand") {
      parseCommand("pageCommand", pageCommand, tokens, fileName, line);
    } else if (cmd == "launchCommand") {
      parseCommand("launchCommand", launchCommand, tokens, fileName, line);
    } else if (cmd == "urlCommand") {
      parseCommand("urlCommand", urlCommand, tokens, fileName, line);
    } else if (cmd == "movieCommand") {
      parseCommand("movieCommand", movieCommand, tokens, fileName, line);
    } else if (cmd == "bind") {
      parseBind(tokens, fileName, line);
    } else if (cmd == "unbind") {
      parseUnbind(tokens, fileName, line);
    } else if (cmd == "printCommands") {
      parseYesNo("printCommands", globalParam(setPrintCommands),
                 tokens, fileName, line);
    } else if (cmd == "errQuiet") {
      parseYesNo("errQuiet", globalParam(setErrQuiet), tokens, fileName, line);
    } else {
      error(errConfig, -1, "Unknown config file command '{0:s}' ({1:s}:{2:d})",
            cmd.c_str(), fileName.c_str(), line);
      if (cmd == "displayFontX" ||
          cmd == "displayNamedCIDFontX" ||
          cmd == "displayCIDFontX") {
        error(errConfig, -1, "Xpdf no longer supports X fonts");
      } else if (cmd == "fontpath" || cmd == "fontmap") {
        error(errConfig, -1,
              "The config file format has changed since Xpdf 0.9x");
      } else if (cmd == "antialias" ||
                 cmd == "antialiasPrinting" ||
                 cmd == "cMapDir" ||
                 cmd == "cidToUnicode" ||
                 cmd == "disableFreeTypeHinting" ||
                 cmd == "drawAnnotations" ||
                 cmd == "enableFreeType" ||
                 cmd == "enableT1lib" ||
                 cmd == "enableXFA" ||
                 cmd == "fontDir" ||
                 cmd == "fontFileCC" ||
                 cmd == "freetypeControl" ||
                 cmd == "mapExtTrueTypeFontsViaUnicode" ||
                 cmd == "mapNumericCharNames" ||
                 cmd == "mapUnknownCharNames" ||
                 cmd == "minLineWidth" ||
                 cmd == "nameToUnicode" ||
                 cmd == "psASCIIHex" ||
                 cmd == "psAlwaysRasterize" ||
                 cmd == "psCenter" ||
                 cmd == "psEmbedCIDPostScriptFonts" ||
                 cmd == "psEmbedCIDTrueTypeFonts" ||
                 cmd == "psEmbedTrueTypeFonts" ||
                 cmd == "psEmbedType1Fonts" ||
                 cmd == "psFontPassthrough" ||
                 cmd == "psLZW" ||
                 cmd == "psMinLineWidth" ||
                 cmd == "psOPI" ||
                 cmd == "psPreload" ||
                 cmd == "psRasterMono" ||
                 cmd == "psRasterResolution" ||
                 cmd == "psRasterSliceSize" ||
                 cmd == "psResidentFont" ||
                 cmd == "psResidentFont16" ||
                 cmd == "psResidentFontCC" ||
                 cmd == "psUncompressPreloadedImages" ||
                 cmd == "psUseCropBoxAsPage" ||
                 cmd == "screenBlackThreshold" ||
                 cmd == "screenDotRadius" ||
                 cmd == "screenGamma" ||
                 cmd == "screenSize" ||
                 cmd == "screenType" ||
                 cmd == "screenWhiteThreshold" ||
                 cmd == "strokeAdjust" ||
                 cmd == "t1libControl" ||
                 cmd == "textEOL" ||
                 cmd == "textKeepTinyChars" ||
                 cmd == "textPageBreaks" ||
                 cmd == "toUnicodeDir" ||
                 cmd == "unicodeMap" ||
                 cmd == "unicodeToUnicode" ||
                 cmd == "vectorAntialias") {
        error(errConfig, -1,
              "This option is not supported by the Poppler version of xpdf");
      }
    }
  }
}

void XPDFParams::parseFontFile(const StringList& tokens,
                               const std::string& fileName, int line) {
  if (tokens.size() != 3) {
    error(errConfig, -1, "Bad 'fontFile' config file command ({0:s}:{1:d})",
          fileName.c_str(), line);
    return;
  }
#ifdef ADDFONTFILE_NO_OWN
  auto fontName = makeGooStringPtr(tokens[1]);
  auto fontFile = makeGooStringPtr(tokens[2]);
  globalParams->addFontFile(fontName.get(), fontFile.get());
#else
  // These are owning pointers before Poppler 0.65.
  globalParams->addFontFile(makeGooString(tokens[1]), makeGooString(tokens[2]));
#endif
}

void XPDFParams::parsePSFile(const StringList& tokens,
                             const std::string& fileName, int line) {
  if (tokens.size() != 2) {
    error(errConfig, -1, "Bad 'psFile' config file command ({0:s}:{1:d})",
          fileName.c_str(), line);
    return;
  }
  setPSFile(tokens[1]);
}

void XPDFParams::parsePSPaperSize(const StringList& tokens,
                                  const std::string& fileName, int line) {
  if (tokens.size() == 2) {
    if (!setPSPaperSize(tokens[1])) {
      error(errConfig, -1,
            "Bad 'psPaperSize' config file command ({0:s}:{1:d})",
            fileName.c_str(), line);
    }
  } else if (tokens.size() == 3) {
    psPaperWidth = atoi(tokens[1].c_str());
    psPaperHeight = atoi(tokens[2].c_str());
    psImageableLLX = psImageableLLY = 0;
    psImageableURX = psPaperWidth;
    psImageableURY = psPaperHeight;
  } else {
    error(errConfig, -1, "Bad 'psPaperSize' config file command ({0:s}:{1:d})",
          fileName.c_str(), line);
  }
}

void XPDFParams::parsePSImageableArea(const StringList& tokens,
                                      const std::string& fileName, int line) {
  if (tokens.size() != 5) {
    error(errConfig, -1,
          "Bad 'psImageableArea' config file command ({0:s}:{1:d})",
          fileName.c_str(), line);
    return;
  }
  psImageableLLX = atoi(tokens[1].c_str());
  psImageableLLY = atoi(tokens[2].c_str());
  psImageableURX = atoi(tokens[3].c_str());
  psImageableURY = atoi(tokens[4].c_str());
}

void XPDFParams::parsePSLevel(const StringList& tokens,
                              const std::string& fileName, int line) {
  if (tokens.size() != 2) {
    error(errConfig, -1, "Bad 'psLevel' config file command ({0:s}:{1:d})",
          fileName.c_str(), line);
    return;
  }
  const auto& tok = tokens[1];
  if (tok == "level1") {
    setPSLevel(psLevel1);
  } else if (tok == "level1sep") {
    setPSLevel(psLevel1Sep);
  } else if (tok == "level2") {
    setPSLevel(psLevel2);
  } else if (tok == "level2sep") {
    setPSLevel(psLevel2Sep);
  } else if (tok == "level3") {
    setPSLevel(psLevel3);
  } else if (tok == "level3Sep") {
    setPSLevel(psLevel3Sep);
  } else {
    error(errConfig, -1, "Bad 'psLevel' config file command ({0:s}:{1:d})",
          fileName.c_str(), line);
  }
}

void XPDFParams::parseInitialZoom(const StringList& tokens,
                                  const std::string& fileName, int line) {
  if (tokens.size() != 2) {
    error(errConfig, -1, "Bad 'initialZoom' config file command ({0:s}:{1:d})",
          fileName.c_str(), line);
    return;
  }
  initialZoom = tokens[1];
}

void XPDFParams::parseBind(const StringList& tokens,
                           const std::string& fileName, int line) {
  int code, mods, context;

  if (tokens.size() < 4) {
    error(errConfig, -1, "Bad 'bind' config file command ({0:s}:{1:d})",
          fileName.c_str(), line);
    return;
  }
  if (!parseKey(tokens[1], tokens[2],
                &code, &mods, &context,
                "bind", tokens, fileName, line)) {
    return;
  }
  for (auto it = keyBindings.begin(); it != keyBindings.end(); it++) {
    const auto& binding = *it;
    if (binding.code == code &&
        binding.mods == mods &&
        binding.context == context) {
      keyBindings.erase(it);
      break;
    }
  }
  StringList cmds(tokens.begin() + 3, tokens.end());
  keyBindings.emplace_back(code, mods, context, cmds);
}

void XPDFParams::parseUnbind(const StringList& tokens,
                             const std::string& fileName, int line) {
  int code, mods, context;

  if (tokens.size() != 3) {
    error(errConfig, -1, "Bad 'unbind' config file command ({0:s}:{1:d})",
          fileName.c_str(), line);
    return;
  }
  if (!parseKey(tokens[1], tokens[2],
                &code, &mods, &context,
                "unbind", tokens, fileName, line)) {
    return;
  }
  for (auto it = keyBindings.begin(); it != keyBindings.end(); it++) {
    const auto& binding = *it;
    if (binding.code == code &&
        binding.mods == mods &&
        binding.context == context) {
      keyBindings.erase(it);
      break;
    }
  }
}

bool XPDFParams::parseKey(const std::string& modKeyStr,
                          const std::string& contextStr,
                          int *code, int *mods, int *context,
                          const char *cmdName,
                          const StringList& tokens,
                          const std::string& fileName, int line) {
  const char *p0;
  int btn;

  *mods = xpdfKeyModNone;
  p0 = modKeyStr.c_str();
  while (1) {
    if (!strncmp(p0, "shift-", 6)) {
      *mods |= xpdfKeyModShift;
      p0 += 6;
    } else if (!strncmp(p0, "ctrl-", 5)) {
      *mods |= xpdfKeyModCtrl;
      p0 += 5;
    } else if (!strncmp(p0, "alt-", 4)) {
      *mods |= xpdfKeyModAlt;
      p0 += 4;
    } else {
      break;
    }
  }

  if (!strcmp(p0, "space")) {
    *code = ' ';
  } else if (!strcmp(p0, "tab")) {
    *code = xpdfKeyCodeTab;
  } else if (!strcmp(p0, "return")) {
    *code = xpdfKeyCodeReturn;
  } else if (!strcmp(p0, "enter")) {
    *code = xpdfKeyCodeEnter;
  } else if (!strcmp(p0, "backspace")) {
    *code = xpdfKeyCodeBackspace;
  } else if (!strcmp(p0, "insert")) {
    *code = xpdfKeyCodeInsert;
  } else if (!strcmp(p0, "delete")) {
    *code = xpdfKeyCodeDelete;
  } else if (!strcmp(p0, "home")) {
    *code = xpdfKeyCodeHome;
  } else if (!strcmp(p0, "end")) {
    *code = xpdfKeyCodeEnd;
  } else if (!strcmp(p0, "pgup")) {
    *code = xpdfKeyCodePgUp;
  } else if (!strcmp(p0, "pgdn")) {
    *code = xpdfKeyCodePgDn;
  } else if (!strcmp(p0, "left")) {
    *code = xpdfKeyCodeLeft;
  } else if (!strcmp(p0, "right")) {
    *code = xpdfKeyCodeRight;
  } else if (!strcmp(p0, "up")) {
    *code = xpdfKeyCodeUp;
  } else if (!strcmp(p0, "down")) {
    *code = xpdfKeyCodeDown;
  } else if (!strcmp(p0, "escape")) {
    *code = xpdfKeyCodeEscape;
  } else if (p0[0] == 'f' && p0[1] >= '1' && p0[1] <= '9' && !p0[2]) {
    *code = xpdfKeyCodeF1 + (p0[1] - '1');
  } else if (p0[0] == 'f' &&
             ((p0[1] >= '1' && p0[1] <= '2' && p0[2] >= '0' && p0[2] <= '9') ||
              (p0[1] == '3' && p0[2] >= '0' && p0[2] <= '5')) &&
             !p0[3]) {
    *code = xpdfKeyCodeF1 + 10 * (p0[1] - '0') + (p0[2] - '0') - 1;
  } else if (!strncmp(p0, "mousePress", 10) &&
             p0[10] >= '0' && p0[10] <= '9' &&
             (!p0[11] || (p0[11] >= '0' && p0[11] <= '9' && !p0[12])) &&
             (btn = atoi(p0 + 10)) >= 1 && btn <= 32) {
    *code = xpdfKeyCodeMousePress1 + btn - 1;
  } else if (!strncmp(p0, "mouseRelease", 12) &&
             p0[12] >= '0' && p0[12] <= '9' &&
             (!p0[13] || (p0[13] >= '0' && p0[13] <= '9' && !p0[14])) &&
             (btn = atoi(p0 + 12)) >= 1 && btn <= 32) {
    *code = xpdfKeyCodeMouseRelease1 + btn - 1;
  } else if (*p0 >= 0x20 && *p0 <= 0x7e && !p0[1]) {
    *code = (int)*p0;
  } else {
    error(errConfig, -1,
          "Bad key/modifier in '{0:s}' config file command ({1:s}:{2:d})",
          cmdName, fileName.c_str(), line);
    return false;
  }

  p0 = contextStr.c_str();
  if (!strcmp(p0, "any")) {
    *context = xpdfKeyContextAny;
  } else {
    *context = xpdfKeyContextAny;
    while (1) {
      if (!strncmp(p0, "fullScreen", 10)) {
        *context |= xpdfKeyContextFullScreen;
        p0 += 10;
      } else if (!strncmp(p0, "window", 6)) {
        *context |= xpdfKeyContextWindow;
        p0 += 6;
      } else if (!strncmp(p0, "continuous", 10)) {
        *context |= xpdfKeyContextContinuous;
        p0 += 10;
      } else if (!strncmp(p0, "singlePage", 10)) {
        *context |= xpdfKeyContextSinglePage;
        p0 += 10;
      } else if (!strncmp(p0, "overLink", 8)) {
        *context |= xpdfKeyContextOverLink;
        p0 += 8;
      } else if (!strncmp(p0, "offLink", 7)) {
        *context |= xpdfKeyContextOffLink;
        p0 += 7;
      } else if (!strncmp(p0, "outline", 7)) {
        *context |= xpdfKeyContextOutline;
        p0 += 7;
      } else if (!strncmp(p0, "mainWin", 7)) {
        *context |= xpdfKeyContextMainWin;
        p0 += 7;
      } else if (!strncmp(p0, "scrLockOn", 9)) {
        *context |= xpdfKeyContextScrLockOn;
        p0 += 9;
      } else if (!strncmp(p0, "scrLockOff", 10)) {
        *context |= xpdfKeyContextScrLockOff;
        p0 += 10;
      } else {
        error(errConfig, -1,
              "Bad context in '{0:s}' config file command ({1:s}:{2:d})",
              cmdName, fileName.c_str(), line);
        return false;
      }
      if (!*p0) {
        break;
      }
      if (*p0 != ',') {
        error(errConfig, -1,
              "Bad context in '{0:s}' config file command ({1:s}:{2:d})",
              cmdName, fileName.c_str(), line);
        return false;
      }
      ++p0;
    }
  }

  return true;
}

void XPDFParams::parseCommand(const char *cmdName, std::string& val,
                              const StringList& tokens,
                              const std::string& fileName, int line) {
  if (tokens.size() != 2) {
    error(errConfig, -1, "Bad '{0:s}' config file command ({1:s}:{2:d})",
          cmdName, fileName.c_str(), line);
    return;
  }
  val = tokens[1];
}

void XPDFParams::parseString(const char *cmdName, const Param<char *> &param,
                             const StringList& tokens,
                             const std::string& fileName, int line) {
  if (tokens.size() != 2) {
    error(errConfig, -1, "Bad '{0:s}' config file command ({1:s}:{2:d})",
          cmdName, fileName.c_str(), line);
    return;
  }
  if (!param.set((char *)tokens[1].c_str())) {
    error(errConfig, -1, "Bad '{0:s}' config file command ({1:s}:{2:d})",
          cmdName, fileName.c_str(), line);
  }
}

void XPDFParams::parseString(const char *cmdName,
                             const Param<const char *> &param,
                             const StringList& tokens,
                             const std::string& fileName, int line) {
  if (tokens.size() != 2) {
    error(errConfig, -1, "Bad '{0:s}' config file command ({1:s}:{2:d})",
          cmdName, fileName.c_str(), line);
    return;
  }
  if (!param.set(tokens[1].c_str())) {
    error(errConfig, -1, "Bad '{0:s}' config file command ({1:s}:{2:d})",
          cmdName, fileName.c_str(), line);
  }
}

void XPDFParams::parseYesNo(const char *cmdName, const Param<bool> &param,
                            const StringList& tokens,
                            const std::string& fileName, int line) {
  if (tokens.size() != 2) {
    error(errConfig, -1, "Bad '{0:s}' config file command ({1:s}:{2:d})",
          cmdName, fileName.c_str(), line);
    return;
  }
  if (tokens[1] == "yes") {
    param.set(true);
  } else if (tokens[1] == "no") {
    param.set(false);
  } else {
    error(errConfig, -1, "Bad '{0:s}' config file command ({1:s}:{2:d})",
          cmdName, fileName.c_str(), line);
  }
}

void XPDFParams::parseInteger(const char *cmdName, const Param<int> &param,
                              const StringList& tokens,
                              const std::string& fileName, int line) {
  unsigned int i;

  if (tokens.size() != 2) {
    error(errConfig, -1, "Bad '{0:s}' config file command ({1:s}:{2:d})",
          cmdName, fileName.c_str(), line);
    return;
  }
  const auto& tok = tokens[1];
  if (tok.size() == 0) {
    error(errConfig, -1, "Bad '{0:s}' config file command ({1:s}:{2:d})",
          cmdName, fileName.c_str(), line);
    return;
  }
  if (tok[0] == '-') {
    i = 1;
  } else {
    i = 0;
  }
  for (; i < tok.size(); ++i) {
    if (tok[i] < '0' || tok[i] > '9') {
      error(errConfig, -1, "Bad '{0:s}' config file command ({1:s}:{2:d})",
            cmdName, fileName.c_str(), line);
      return;
    }
  }
  param.set(atoi(tok.c_str()));
}

void XPDFParams::parseFloat(const char *cmdName, const Param<double> &param,
                            const StringList& tokens,
                            const std::string& fileName, int line) {
  unsigned int i;

  if (tokens.size() != 2) {
    error(errConfig, -1, "Bad '{0:s}' config file command ({1:s}:{2:d})",
          cmdName, fileName.c_str(), line);
    return;
  }
  const auto& tok = tokens[1];
  if (tok.size() == 0) {
    error(errConfig, -1, "Bad '{0:s}' config file command ({1:s}:{2:d})",
          cmdName, fileName.c_str(), line);
    return;
  }
  if (tok[0] == '-') {
    i = 1;
  } else {
    i = 0;
  }
  for (; i < tok.size(); ++i) {
    if (!((tok[i] >= '0' && tok[i] <= '9') ||
          tok[i] == '.')) {
      error(errConfig, -1, "Bad '{0:s}' config file command ({1:s}:{2:d})",
            cmdName, fileName.c_str(), line);
      return;
    }
  }
  param.set(atof(tok.c_str()));
}

XPDFParams::~XPDFParams() {
#if MULTITHREADED
  gDestroyMutex(&mutex);
#endif
}

//------------------------------------------------------------------------

//------------------------------------------------------------------------
// accessors
//------------------------------------------------------------------------

std::string XPDFParams::getPSFile() {
  std::string s;

  lockXPDFParams;
  s = psFile;
  unlockXPDFParams;
  return s;
}

int XPDFParams::getPSPaperWidth() {
  int w;

  lockXPDFParams;
  w = psPaperWidth;
  unlockXPDFParams;
  return w;
}

int XPDFParams::getPSPaperHeight() {
  int h;

  lockXPDFParams;
  h = psPaperHeight;
  unlockXPDFParams;
  return h;
}

void XPDFParams::getPSImageableArea(int *llx, int *lly, int *urx, int *ury) {
  lockXPDFParams;
  *llx = psImageableLLX;
  *lly = psImageableLLY;
  *urx = psImageableURX;
  *ury = psImageableURY;
  unlockXPDFParams;
}

bool XPDFParams::getPSCrop() {
  bool f;

  lockXPDFParams;
  f = psCrop;
  unlockXPDFParams;
  return f;
}

bool XPDFParams::getPSDuplex() {
  bool d;

  lockXPDFParams;
  d = psDuplex;
  unlockXPDFParams;
  return d;
}

PSLevel XPDFParams::getPSLevel() {
  PSLevel level;

  lockXPDFParams;
  level = psLevel;
  unlockXPDFParams;
  return level;
}

std::string XPDFParams::getInitialZoom() {
  std::string s;

  lockXPDFParams;
  s = initialZoom;
  unlockXPDFParams;
  return s;
}

bool XPDFParams::getContinuousView() {
  bool f;

  lockXPDFParams;
  f = continuousView;
  unlockXPDFParams;
  return f;
}

const StringList &XPDFParams::getKeyBinding(int code, int mods, int context) {
  int modMask;

  lockXPDFParams;
  // for ASCII chars, ignore the shift modifier
  modMask = code <= 0xff ? ~xpdfKeyModShift : ~0;
  for (const auto& binding: keyBindings) {
    if (binding.code == code &&
        (binding.mods & modMask) == (mods & modMask) &&
        (~binding.context | context) == ~0) {
      unlockXPDFParams;
      return binding.cmds;
    }
  }
  unlockXPDFParams;
  // no binding found, so return a static empty list
  static const StringList noCmds;
  return noCmds;
}

//------------------------------------------------------------------------
// functions to set parameters
//------------------------------------------------------------------------

void XPDFParams::setPSFile(const std::string& file) {
  lockXPDFParams;
  psFile = file;
  unlockXPDFParams;
}

bool XPDFParams::setPSPaperSize(const std::string& size) {
  lockXPDFParams;
  if (size == "match") {
    psPaperWidth = psPaperHeight = -1;
  } else if (size == "letter") {
    psPaperWidth = 612;
    psPaperHeight = 792;
  } else if (size == "legal") {
    psPaperWidth = 612;
    psPaperHeight = 1008;
  } else if (size == "A4") {
    psPaperWidth = 595;
    psPaperHeight = 842;
  } else if (size == "A3") {
    psPaperWidth = 842;
    psPaperHeight = 1190;
  } else {
    unlockXPDFParams;
    return false;
  }
  psImageableLLX = psImageableLLY = 0;
  psImageableURX = psPaperWidth;
  psImageableURY = psPaperHeight;
  unlockXPDFParams;
  return true;
}

void XPDFParams::setPSPaperWidth(int width) {
  lockXPDFParams;
  psPaperWidth = width;
  psImageableLLX = 0;
  psImageableURX = psPaperWidth;
  unlockXPDFParams;
}

void XPDFParams::setPSPaperHeight(int height) {
  lockXPDFParams;
  psPaperHeight = height;
  psImageableLLY = 0;
  psImageableURY = psPaperHeight;
  unlockXPDFParams;
}

void XPDFParams::setPSImageableArea(int llx, int lly, int urx, int ury) {
  lockXPDFParams;
  psImageableLLX = llx;
  psImageableLLY = lly;
  psImageableURX = urx;
  psImageableURY = ury;
  unlockXPDFParams;
}

void XPDFParams::setPSCrop(bool crop) {
  lockXPDFParams;
  psCrop = crop;
  unlockXPDFParams;
}

void XPDFParams::setPSDuplex(bool duplex) {
  lockXPDFParams;
  psDuplex = duplex;
  unlockXPDFParams;
}

void XPDFParams::setPSLevel(PSLevel level) {
  lockXPDFParams;
  psLevel = level;
  unlockXPDFParams;
}

void XPDFParams::setInitialZoom(const std::string& s) {
  lockXPDFParams;
  initialZoom = s;
  unlockXPDFParams;
}

void XPDFParams::setContinuousView(bool cont) {
  lockXPDFParams;
  continuousView = cont;
  unlockXPDFParams;
}

void XPDFParams::setPageCommand(const std::string& cmd) {
  lockXPDFParams;
  pageCommand = cmd;
  unlockXPDFParams;
}
