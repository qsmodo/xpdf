AC_INIT([xpopple], [3.03.0], [ats@offog.org])
AM_INIT_AUTOMAKE([foreign subdir-objects])

AC_PROG_CXX
AC_LANG([C++])

PKG_CHECK_MODULES([POPPLER], [poppler >= 0.19.0])
CPPFLAGS="$CPPFLAGS $POPPLER_CFLAGS"
LIBS="$LIBS $POPPLER_LIBS"

dnl Check for versions that introduced non-backwards-compatible API changes.
PKG_CHECK_EXISTS([poppler >= 0.23.0], [
  AC_DEFINE([STARTPAGE_XREF])
])
PKG_CHECK_EXISTS([poppler >= 0.25.0], [
  AC_DEFINE([PSOUTPUTDEV_NOCROP])
])
PKG_CHECK_EXISTS([poppler >= 0.30.0], [
  AC_DEFINE([PSOUTPUTDEV_PAGE_LIST])
])
PKG_CHECK_EXISTS([poppler >= 0.58.0], [
  AC_DEFINE([OBJECT_NO_FREE])
])
PKG_CHECK_EXISTS([poppler >= 0.64.0], [
  AC_DEFINE([POPPLER_64_CONST_API])
])
PKG_CHECK_EXISTS([poppler >= 0.65.0], [
  AC_DEFINE([ADDFONTFILE_NO_OWN])
])
PKG_CHECK_EXISTS([poppler >= 0.72.0], [
  AC_DEFINE([GOOSTRING_NO_GETCSTRING])
])
PKG_CHECK_EXISTS([poppler >= 0.76.0], [
  AC_DEFINE([NO_GOOLIST])
  AC_DEFINE([FINDPAGE_REF])
])
PKG_CHECK_EXISTS([poppler >= 0.83.0], [
  AC_DEFINE([GLOBALPARAMS_UNIQUE_PTR])
])
PKG_CHECK_EXISTS([poppler >= 0.84.0], [
  AC_DEFINE([TEXTPAGE_GETTEXT_EOL])
])
PKG_CHECK_EXISTS([poppler >= 0.85.0], [
  AC_DEFINE([UNICODEMAP_NO_REFCOUNT])
])
PKG_CHECK_EXISTS([poppler >= 0.90.0], [
  AC_DEFINE([PSOUTPUTDEV_FORCERASTERIZE])
])
PKG_CHECK_EXISTS([poppler >= 20.12.0], [
  AC_DEFINE([GLOBALPARAMS_NO_PSLEVEL])
])

AC_SEARCH_LIBS([XmCreateFormDialog], [Xm], [], [
  AC_MSG_ERROR([Motif is required])
])

PKG_CHECK_MODULES([XT], [xt x11])
CPPFLAGS="$CPPFLAGS $XT_CFLAGS"
LIBS="$LIBS $XT_LIBS"

AC_SEARCH_LIBS([pthread_mutexattr_settype], [pthread], [], [
  AC_MSG_ERROR([pthreads is required])
])

dnl Motif's API uses char * parameters for strings.
CPPFLAGS="$CPPFLAGS -Wno-write-strings"

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
