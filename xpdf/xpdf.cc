//========================================================================
//
// xpdf.cc
//
// Copyright 1996-2003 Glyph & Cog, LLC
// Copyright 2014-2020 Adam Sampson <ats@offog.org>
//
//========================================================================

#include <poppler-config.h>
#include <memory>
#include <string>
#include <goo/GooString.h>
#include "parseargs.h"
#include <goo/gfile.h>
#include <goo/gmem.h>
#include "GlobalParams.h"
#include "XPDFParams.h"
#include "Object.h"
#include "XPDFApp.h"
#include "config.h"

//------------------------------------------------------------------------
// command line options
//------------------------------------------------------------------------

static bool contView = false;
static char pageCmdArg[256] = "";
static char psFileArg[256];
static char paperSize[15] = "";
static int paperWidth = 0;
static int paperHeight = 0;
static bool level1 = false;
static char textEncName[128] = "";
static char ownerPasswordArg[33] = "\001";
static char userPasswordArg[33] = "\001";
static bool fullScreen = false;
static char remoteName[100] = "xpdf_";
static char remoteCmd[512] = "";
static bool doRemoteReload = false;
static bool doRemoteRaise = false;
static bool doRemoteQuit = false;
static bool printCommands = false;
static bool quiet = false;
static char cfgFileName[256] = "";
static bool printVersion = false;
static bool printHelp = false;

static ArgDesc argDesc[] = {
  {"-g",          argStringDummy, NULL,           0,
   "initial window geometry"},
  {"-geometry",   argStringDummy, NULL,           0,
   "initial window geometry"},
  {"-title",      argStringDummy, NULL,           0,
   "window title"},
  {"-cmap",       argFlagDummy,   NULL,           0,
   "install a private colormap"},
  {"-rgb",        argIntDummy,    NULL,           0,
   "biggest RGB cube to allocate (default is 5)"},
  {"-rv",         argFlagDummy,   NULL,           0,
   "reverse video"},
  {"-papercolor", argStringDummy, NULL,           0,
   "color of paper background"},
  {"-mattecolor", argStringDummy, NULL,           0,
   "color of background outside actual page"},
  {"-z",          argStringDummy, NULL,           0,
   "initial zoom level (percent, 'page', 'width')"},
  {"-cont",       argFlag,        &contView,      0,
   "start in continuous view mode" },
  {"-pagecmd",    argString,      pageCmdArg,     sizeof(pageCmdArg),
   "command to execute on page changes" },
  {"-ps",         argString,      psFileArg,      sizeof(psFileArg),
   "default PostScript file name or command"},
  {"-paper",      argString,      paperSize,      sizeof(paperSize),
   "paper size (letter, legal, A4, A3, match)"},
  {"-paperw",     argInt,         &paperWidth,    0,
   "paper width, in points"},
  {"-paperh",     argInt,         &paperHeight,   0,
   "paper height, in points"},
  {"-level1",     argFlag,        &level1,        0,
   "generate Level 1 PostScript"},
  {"-enc",        argString,      textEncName,    sizeof(textEncName),
   "output text encoding name"},
  {"-opw",        argString,      ownerPasswordArg, sizeof(ownerPasswordArg),
   "owner password (for encrypted files)"},
  {"-upw",        argString,      userPasswordArg, sizeof(userPasswordArg),
   "user password (for encrypted files)"},
  {"-fullscreen", argFlag,        &fullScreen,    0,
   "run in full-screen (presentation) mode"},
  {"-remote",     argString,      remoteName + 5, sizeof(remoteName) - 5,
   "start/contact xpdf remote server with specified name"},
  {"-exec",       argString,      remoteCmd,      sizeof(remoteCmd),
   "execute command on xpdf remote server (with -remote only)"},
  {"-reload",     argFlag,        &doRemoteReload, 0,
   "reload xpdf remote server window (with -remote only)"},
  {"-raise",      argFlag,        &doRemoteRaise, 0,
   "raise xpdf remote server window (with -remote only)"},
  {"-quit",       argFlag,        &doRemoteQuit,  0,
   "kill xpdf remote server (with -remote only)"},
  {"-cmd",        argFlag,        &printCommands, 0,
   "print commands as they're executed"},
  {"-q",          argFlag,        &quiet,         0,
   "don't print any messages or errors"},
  {"-cfg",        argString,      cfgFileName,    sizeof(cfgFileName),
   "configuration file to use in place of .xpdfrc"},
  {"-v",          argFlag,        &printVersion,  0,
   "print copyright and version info"},
  {"-h",          argFlag,        &printHelp,     0,
   "print usage information"},
  {"-help",       argFlag,        &printHelp,     0,
   "print usage information"},
  {"--help",      argFlag,        &printHelp,     0,
   "print usage information"},
  {"-?",          argFlag,        &printHelp,     0,
   "print usage information"},
  {"-aa",         argObsolete1,   NULL,           0,
   NULL},
  {"-aaVector",   argObsolete1,   NULL,           0,
   NULL},
  {"-eol",        argObsolete1,   NULL,           0,
   NULL},
  {"-freetype",   argObsolete1,   NULL,           0,
   NULL},
  {"-t1lib",      argObsolete1,   NULL,           0,
   NULL},
  {NULL}
};

//------------------------------------------------------------------------

int main(int argc, char *argv[]) {
  std::unique_ptr<XPDFApp> app;
  std::unique_ptr<std::string> fileName;
  int pg = 1;
  std::unique_ptr<std::string> destName;
  std::unique_ptr<std::string> userPassword, ownerPassword;
  bool ok;

  // parse args
  ok = parseArgs(argDesc, &argc, argv);
  if (!ok || printVersion || printHelp) {
    fprintf(stderr, "xpdf version %s\n", xpdfVersion);
    fprintf(stderr, "%s\n", xpdfCopyright);
    if (!printVersion) {
      printUsage("xpdf", "[<PDF-file> [<page> | +<dest>]]", argDesc);
    }
    return 99;
  }

  // read config file
#ifdef GLOBALPARAMS_UNIQUE_PTR
  globalParams = std::make_unique<GlobalParams>();
#else
  auto globalParamsPtr = std::make_unique<GlobalParams>();
  globalParams = globalParamsPtr.get();
#endif
  xpdfParams = std::make_unique<XPDFParams>(cfgFileName);
  globalParams->setupBaseFonts(NULL);
  if (contView) {
    xpdfParams->setContinuousView(contView);
  }
  if (pageCmdArg[0]) {
    xpdfParams->setPageCommand(pageCmdArg);
  }
  if (psFileArg[0]) {
    xpdfParams->setPSFile(psFileArg);
  }
  if (paperSize[0]) {
    if (!xpdfParams->setPSPaperSize(paperSize)) {
      fprintf(stderr, "Invalid paper size\n");
    }
  } else {
    if (paperWidth) {
      xpdfParams->setPSPaperWidth(paperWidth);
    }
    if (paperHeight) {
      xpdfParams->setPSPaperHeight(paperHeight);
    }
  }
  if (level1) {
    xpdfParams->setPSLevel(psLevel1);
  }
  if (textEncName[0]) {
    globalParams->setTextEncoding(textEncName);
  }
  if (printCommands) {
    globalParams->setPrintCommands(printCommands);
  }
  if (quiet) {
    globalParams->setErrQuiet(quiet);
  }

  // create the XPDFApp object
  app = std::make_unique<XPDFApp>(&argc, argv);

  // the initialZoom parameter can be set in either the config file or
  // as an X resource (or command line arg)
  if (app->getInitialZoom() != "") {
    xpdfParams->setInitialZoom(app->getInitialZoom());
  }

  // check command line
  ok = ok && argc >= 1 && argc <= 3;
  if (remoteCmd[0]) {
    ok = ok && remoteName[5] && !doRemoteReload && !doRemoteRaise &&
         !doRemoteQuit && argc == 1;
  }
  if (doRemoteReload) {
    ok = ok && remoteName[5] && !doRemoteQuit && argc == 1;
  }
  if (doRemoteRaise) {
    ok = ok && remoteName[5] && !doRemoteQuit;
  }
  if (doRemoteQuit) {
    ok = ok && remoteName[5] && argc == 1;
  }
  if (!ok || printVersion || printHelp) {
    fprintf(stderr, "xpdf version %s\n", xpdfVersion);
    fprintf(stderr, "%s\n", xpdfCopyright);
    if (!printVersion) {
      printUsage("xpdf", "[<PDF-file> [<page> | +<dest>]]", argDesc);
    }
    return 99;
  }
  if (argc >= 2) {
    fileName = std::make_unique<std::string>(argv[1]);
  }
  if (argc == 3) {
    if (argv[2][0] == '+') {
      destName = std::make_unique<std::string>(&argv[2][1]);
    } else {
      pg = atoi(argv[2]);
      if (pg < 0) {
	fprintf(stderr, "Invalid page number (%d)\n", pg);
	return 99;
      }
    }
  }

  // handle remote server stuff
  if (remoteName[5]) {
    app->setRemoteName(remoteName);
    if (app->remoteServerRunning()) {
      if (fileName) {
	if (destName) {
	  app->remoteOpenAtDest(*fileName, *destName, doRemoteRaise);
	} else {
	  app->remoteOpen(*fileName, pg, doRemoteRaise);
	}
      } else if (remoteCmd[0]) {
	app->remoteExec(remoteCmd);
      } else if (doRemoteReload) {
	app->remoteReload(doRemoteRaise);
      } else if (doRemoteRaise) {
	app->remoteRaise();
      } else if (doRemoteQuit) {
	app->remoteQuit();
      }
      return 0;
    }
    if (doRemoteQuit) {
      return 0;
    }
  }

  // set options
  app->setFullScreen(fullScreen);

  // check for password string(s)
  if (ownerPasswordArg[0] != '\001') {
    ownerPassword = std::make_unique<std::string>(ownerPasswordArg);
  }
  if (userPasswordArg[0] != '\001') {
    userPassword = std::make_unique<std::string>(userPasswordArg);
  }

  // open the file and run the main loop
  if (!app->open(fileName.get(), pg, destName.get(),
		 ownerPassword.get(), userPassword.get())) {
    return 1;
  }
  app->run();

  return 0;
}
