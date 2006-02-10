if GLX
GLX_LIB = $(top_builddir)/hw/xgl/glxext/libxglglxext.la
endif

LIBXGLX_LIBS =				      \
	$(top_builddir)/hw/xgl/glx/libxglx.la \
	$(GLX_LIB)

AM_CFLAGS =			   \
	-I$(top_srcdir)/hw/xgl/glx \
	-I$(top_srcdir)/hw/xgl	   \
	$(XGLXMODULES_CFLAGS)

libxglx_la_DEPENDENCIES = $(LIBXGLX_LIBS)
libxglx_la_LDFLAGS = -avoid-version
libxglx_la_SOURCES = xglxmodule.c
libxglx_la_LIBADD =	\
	$(LIBXGLX_LIBS)	\
	$(XGLXMODULES_LIBS)

moduledir = @xglmoduledir@

module_LTLIBRARIES = libxglx.la
