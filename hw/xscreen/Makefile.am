bin_PROGRAMS = Xscreen

Xscreen_SOURCES = xs-color.h \
				  xs-color.c \
				  xs-event.c \
				  xs-event.h \
				  xs-font.h \
				  xs-font.c \
				  xs-gc.c \
				  xs-gcops.c \
				  xs-gcops.h \
				  xs-globals.c \
				  xs-globals.h \
				  xs-init.c \
				  xs-input.c \
				  xs-pixmap.c \
				  xs-pixmap.h \
				  xs-screen.c \
				  xs-screen.h \
				  xs-visual.h \
				  xs-visual.c \
				  xs-window.c \
				  xs-window.h \
				  $(top_srcdir)/Xext/dpmsstubs.c \
				  $(top_srcdir)/Xi/stubs.c \
				  $(top_srcdir)/mi/miinitext.c \
				  $(top_srcdir)/fb/fbcmap.c


Xscreen_LDADD = $(XORG_CORE_LIBS) \
				$(XNEST_LIBS) \
				$(XNESTMODULES_LIBS)

INCLUDES = $(XSCREENMODULES_CFLAGS)
AM_CFLAGS = @SERVER_DEFINES@ -DNO_HW_ONLY_EXTS
