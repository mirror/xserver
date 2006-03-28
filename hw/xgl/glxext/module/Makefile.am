AM_CFLAGS =		     \
	@SERVER_DEFINES@     \
	-DHAVE_XGL_CONFIG_H  \
	-DHAVE_DIX_CONFIG_H  \
	$(XGLMODULES_CFLAGS) \
	-I$(top_srcdir)/hw/xgl 

libglxext_la_LDFLAGS = -avoid-version
libglxext_la_SOURCES = glxmodule.c
libglxext_la_LIBADD  = $(top_builddir)/GL/glx/libglx.la \
		       $(top_builddir)/GL/mesa/libGLcore.la
libglxext_modules    = libglxext.la

moduledir = @XGL_MODULE_PATH@

module_LTLIBRARIES = $(libglxext_modules)
