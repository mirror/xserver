bin_PROGRAMS = Xscreen

Xscreen_SOURCES = xs-init.c \
				  xs-input.c \
				  xs-globals.h \
				  xs-globals.c \
				  xs-screen.h \
				  xs-screen.c \
				  $(top_srcdir)/Xext/dpmsstubs.c \
				  $(top_srcdir)/Xi/stubs.c \
				  $(top_srcdir)/mi/miinitext.c \
				  $(top_srcdir)/fb/fbcmap.c


Xscreen_LDADD = $(XORG_CORE_LIBS) \
				$(XNEST_LIBS) \
				$(XNESTMODULES_LIBS)

INCLUDES = $(XSCREENMODULES_CFLAGS)
AM_CFLAGS = @SERVER_DEFINES@ -DNO_HW_ONLY_EXTS
