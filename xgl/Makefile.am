noinst_LTLIBRARIES = libxgl.la

INCLUDES = -I@MESA_SOURCE@/include \
           -I$(top_srcdir)/glx \
           -I$(top_srcdir)/hw/xfree86/os-support \
           -I$(top_srcdir)/hw/xfree86/os-support/bus \
           -I$(top_srcdir)/hw/xfree86/common

# -DXFree86Server is required because the X11 driver in Mesa thinks that
# symbol means "being built in the server"
AM_CFLAGS = \
	@DIX_CFLAGS@ \
	@LIBDRM_CFLAGS@ \
	@DRIPROTO_CFLAGS@ \
	$(XGL_CFLAGS) \
	@GLX_DEFINES@ \
	-DXFree86Server

libxgl_la_SOURCES = \
	xgl.h \
	xglarea.c \
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
