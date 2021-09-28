//========================================================================
//
// gfile-xpdf.cc
//
// Miscellaneous file and directory name manipulation.
//
// Copyright 1996-2003 Glyph & Cog, LLC
//
// This contains the functions that are in xpdf's gfile.cc but not in
// Poppler's.
//
//========================================================================

#include <poppler-config.h>
#include "../xpdf/config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <limits.h>
#include <string.h>
#include <pwd.h>
#include <unistd.h>
#include <goo/GooString.h>
#include <goo/gfile.h>
#include "gfile-xpdf.h"

// Some systems don't define this, so just make it something reasonably
// large.
#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

//------------------------------------------------------------------------

GooString *getHomeDir() {
  char *s;
  struct passwd *pw;
  GooString *ret;

  if ((s = getenv("HOME"))) {
    ret = new GooString(s);
  } else {
    if ((s = getenv("USER")))
      pw = getpwnam(s);
    else
      pw = getpwuid(getuid());
    if (pw)
      ret = new GooString(pw->pw_dir);
    else
      ret = new GooString(".");
  }
  return ret;
}

GooString *xpdfGrabPath(const char *fileName) {
  const char *p;

  if ((p = strrchr(fileName, '/')))
    return new GooString(fileName, p - fileName);
  return new GooString();
}

bool xpdfIsAbsolutePath(const char *path) {
  return path[0] == '/';
}

time_t xpdfGetModTime(const char *fileName) {
  struct stat statBuf;

  if (stat(fileName, &statBuf)) {
    return 0;
  }
  return statBuf.st_mtime;
}

GooString *makePathAbsolute(GooString *path) {
  if (!xpdfIsAbsolutePath(path->getCString())) {
    char buf[PATH_MAX+1];
    if (getcwd(buf, sizeof(buf))) {
      path->insert(0, buf);
    }
  }
  return path;
}
