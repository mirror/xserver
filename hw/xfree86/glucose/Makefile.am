module_LTLIBRARIES = libglucose.la

libglucose_la_LDFLAGS = -avoid-version

INCLUDES = \
	$(XORG_INCS) \
	-I$(srcdir)/../../../glucose \
	-I$(srcdir)/../../../miext/cw

AM_CFLAGS = $(DIX_CFLAGS) $(XORG_CFLAGS)

libglucose_la_SOURCES = \
	glucosemodule.c

libglucose_la_LIBADD = \
	../../../glucose/libglucose.la

include $(top_srcdir)/cpprules.in
