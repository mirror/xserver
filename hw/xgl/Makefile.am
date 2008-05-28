if GLX
GLX_SUBDIRS = glxext
endif

if XGLX
XGLX_SUBDIRS = glx
endif

if XEGL
XEGL_SUBDIRS = egl
endif

DIST_SUBDIRS = glxext glx egl

SUBDIRS = 		\
	.		\
	$(GLX_SUBDIRS)  \
	$(XGLX_SUBDIRS) \
	$(XEGL_SUBDIRS)

AM_CFLAGS = \
	@DIX_CFLAGS@ \
	@LIBDRM_CFLAGS@ \
	@DRIPROTO_CFLAGS@ \
	-DHAVE_XGL_CONFIG_H \
	-DHAVE_DIX_CONFIG_H \
	-I$(top_srcdir)/glx			\
	-I$(top_srcdir)/GL/include		\
	-I@MESA_SOURCE@/include \
	-I@MESA_SOURCE@/src/mesa/glapi \
	-I$(top_srcdir)/xgl \
	$(XGLMODULES_CFLAGS)

EXTRA_DIST = \
	xglmodule.h

Xgl_LDFLAGS = -export-dynamic
Xgl_SOURCES = \
	xglinit.c \
	xglmodule.h \
	xglglx.h \
	xglinput.c \
	xgloutput.c \
	xglparse.c \
	xglscreen.c \
	xglloader.c \
	xglhash.c \
	xglglx.c \
	$(top_srcdir)/mi/miinitext.c \
	$(top_srcdir)/Xext/dpmsstubs.c \
	$(top_srcdir)/Xi/stubs.c \
	$(top_srcdir)/fb/fbcmap.c

XGL_LIBS = \
	$(top_builddir)/xgl/libxgl.la \
	@XGL_LIBS@ \
	$(XSERVER_LIBS)

Xgl_DEPENDENCIES = $(XGL_LIBS)
Xgl_LDADD = $(XGL_LIBS) $(XSERVER_SYS_LIBS) $(XGL_SYS_LIBS)
Xgl_programs = Xgl

bin_PROGRAMS = $(Xgl_programs)
