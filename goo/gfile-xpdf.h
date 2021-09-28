//========================================================================
//
// gfile-xpdf.h
//
// Miscellaneous file and directory name manipulation.
//
// Copyright 1996-2003 Glyph & Cog, LLC
//
//========================================================================

#ifndef GFILE_XPDF_H
#define GFILE_XPDF_H

class GooString;

//------------------------------------------------------------------------

// Get home directory path.
extern GooString *getHomeDir();

// Grab the path from the front of the file name.  If there is no
// directory component in <fileName>, returns an empty string.
extern GooString *xpdfGrabPath(const char *fileName);

// Is this an absolute path or file name?
extern bool xpdfIsAbsolutePath(const char *path);

// Get the modification time for <fileName>.  Returns 0 if there is an
// error.
extern time_t xpdfGetModTime(const char *fileName);

// Make this path absolute by prepending current directory (if path is
// relative) or prepending user's directory (if path starts with '~').
extern GooString *makePathAbsolute(GooString *path);

#endif
