noinst_LIBRARIES = libdmxinput.a

DRVSRCS = dmxbackend.c \
          dmxbackend.h \
          dmxcommon.c \
          dmxcommon.h

DMXSRCS = dmxinputinit.c \
          dmxinputinit.h \
          dmxxinput.c

libdmxinput_a_SOURCES = $(DRVSRCS) \
                        $(DMXSRCS)

AM_CFLAGS = $(DIX_CFLAGS) \
            -I$(top_srcdir)/hw/dmx \
            -DHAVE_DMX_CONFIG_H \
            @DMXMODULES_CFLAGS@
