bin_PROGRAMS = xpdf/xpdf

xpdf_xpdf_CPPFLAGS = \
	-DHAVE_DIRENT_H \
	-DSYSTEM_XPDFRC=\"$(sysconfdir)/xpdf/xpdfrc\" \
	-I$(srcdir)/goo

xpdf_xpdf_SOURCES = \
	goo/gfile-xpdf.cc \
	goo/gfile-xpdf.h \
	goo/parseargs.cc \
	goo/parseargs.h \
	\
	xpdf/CoreOutputDev.cc \
	xpdf/CoreOutputDev.h \
	xpdf/PDFCore.cc \
	xpdf/PDFCore.h \
	xpdf/XPDFApp.cc \
	xpdf/XPDFApp.h \
	xpdf/XPDFCore.cc \
	xpdf/XPDFCore.h \
	xpdf/XPDFParams.cc \
	xpdf/XPDFParams.h \
	xpdf/XPDFTree.cc \
	xpdf/XPDFTree.h \
	xpdf/XPDFTreeP.h \
	xpdf/XPDFViewer.cc \
	xpdf/XPDFViewer.h \
	xpdf/about-text.h \
	xpdf/config.h \
	xpdf/xpdf.cc \
	\
	xpdf/about.xbm \
	xpdf/backArrow.xbm \
	xpdf/backArrowDis.xbm \
	xpdf/dblLeftArrow.xbm \
	xpdf/dblLeftArrowDis.xbm \
	xpdf/dblRightArrow.xbm \
	xpdf/dblRightArrowDis.xbm \
	xpdf/find.xbm \
	xpdf/findDis.xbm \
	xpdf/forwardArrow.xbm \
	xpdf/forwardArrowDis.xbm \
	xpdf/leftArrow.xbm \
	xpdf/leftArrowDis.xbm \
	xpdf/print.xbm \
	xpdf/printDis.xbm \
	xpdf/rightArrow.xbm \
	xpdf/rightArrowDis.xbm \
	xpdf/xpdfIcon.xpm

dist_man_MANS = \
	doc/xpdf.1 \
	doc/xpdfrc.5

EXTRA_DIST = \
	COPYING \
	COPYING3 \
	README \
	TODO \
	doc/sample-xpdfrc \
	misc/hello.pdf \
	misc/xpdf.dt \
	test-builds \
	test-options
