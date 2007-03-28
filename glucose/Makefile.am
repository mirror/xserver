noinst_LTLIBRARIES = libglucose.la

INCLUDES = -I@MESA_SOURCE@/include \
           -I$(top_srcdir)/GL/mesa/glapi \
           -I$(top_srcdir)/GL/glx \
           -I$(top_srcdir)/GL/include \
           -I$(top_srcdir)/hw/xfree86/os-support \
           -I$(top_srcdir)/hw/xfree86/os-support/bus \
           -I$(top_srcdir)/hw/xfree86/common

# -DXFree86Server is required because the X11 driver in Mesa thinks that
# symbol means "being built in the server"
AM_CFLAGS = \
	$(DIX_CFLAGS) \
	$(GLUCOSE_CFLAGS) \
	@GLX_DEFINES@ \
	-DXFree86Server

libglucose_la_SOURCES = \
	glucose.c \
	glucose.h \
	glucose_context.c \
	glucose_drawable.c \
	glucose_format.c \
	glucose_info.c

libglucose_la_LIBADD = $(GLUCOSE_LIBS)


nodist_libglucose_la_SOURCES = \
xgl.h \
xglarea.c \
xglbstore.c \
xglcmap.c \
xglcompose.c \
xglcopy.c \
xglfill.c \
xglgc.c \
xglgeometry.c \
xglget.c \
xglglyph.c \
xglpict.c \
xglpixmap.c \
xglshm.c \
xglsolid.c \
xglsync.c \
xgltile.c \
xgltrap.c \
xglwindow.c \
xglxv.c

EXTRA_DIST = symlink_xgl.sh
