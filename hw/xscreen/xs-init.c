#ifdef HAVE_XSCREEN_CONFIG_H
#include <xnest-config.h>
#endif

#include <stdlib.h>

/* need to include Xmd before XCB stuff, or
 * things get redeclared.*/
#include <X11/Xmd.h>
#include <X11/XCB/xcb.h>
#include <X11/XCB/xcb_aux.h>
#include <X11/XCB/xproto.h>
#include <X11/XCB/shape.h>

#include "gcstruct.h"
#include "window.h"
#include "windowstr.h"
#include "pixmapstr.h"
#include "colormapst.h"
#include "scrnintstr.h"
#include "region.h"

#include "xs-globals.h"
#include "xs-screen.h"

#include "mi.h"

/**
 * DIX hooks for initializing input and output.
 * XKB stuff is not supported yet, since it's currently missing in
 * XCB.
 **/
void InitInput(int argc, char *argv[])
{
    /*shut up GCC.*/
    argc++;
    argv++;
}

void xsInitPixmapFormats(const XCBSetup *setup, PixmapFormatRec fmts[])
{
    XCBFORMAT *bs_fmts; /*formats on backing server*/
    int i;

    bs_fmts = XCBSetupPixmapFormats(setup);
    for (i = 0; i < setup->pixmap_formats_len; i++) {
        fmts[i].depth = bs_fmts[i].depth;
        fmts[i].bitsPerPixel = bs_fmts[i].bits_per_pixel;
        fmts[i].scanlinePad = bs_fmts[i].scanline_pad;
    }
}

/**
 * Here is where we initialize the scren info and open the display.
 **/
void InitOutput(ScreenInfo *si, int argc, char *argv[])
{
    int             screennum;
    const XCBSetup *setup;
    XCBSCREEN      *screen;
    char           *display;

    /*FIXME: add a "-display" option*/
    /*Globals Globals Everywhere.*/
    xsConnection = XCBConnect(NULL, &screennum);

    if (!xsConnection) { /* failure to connect */
        /* prettify the display name */
        display = getenv("DISPLAY");
        if (!display)
            display = "";
        FatalError("Unable to open display \"%s\".\n", display);
    }

    setup = XCBGetSetup(xsConnection);

    /*set up the root window*/
    screen = XCBSetupRootsIter(setup).data;
    xsBackingRoot.window = screen->root;
    /*initialize the ScreenInfo pixmap/image fields*/
    si->imageByteOrder = setup->image_byte_order;
    si->bitmapScanlineUnit = setup->bitmap_format_scanline_unit;
    si->bitmapBitOrder = setup->bitmap_format_bit_order;
    si->numPixmapFormats = setup->pixmap_formats_len;
    xsInitPixmapFormats(setup, si->formats);

    /*initialize the private indexes for stuff*/
    /**
     * NB: If anyone cares about multiple screens in Xscreen,
     * they can add support. I don't care about it, and I'm not
     * going to be putting in barely-tested code
     **/
    si->numScreens = 0;
    /*eww. AddScren increments si->numScreens.*/
    if (AddScreen(xsOpenScreen, argc, argv) < 0)
        FatalError("Failed to initialize screen 0\n");
    /*si->numVideoScreens = ... what do I do with this?;*/

}

/**
 * We don't really need to do cleanup here, at least not yet.
 * There'll be stuff later.
 **/
void ddxGiveUp()
{
    /*FIXME: close display properly*/
}

void AbortDDX()
{
    /*FIXME: close display properly*/
}

/*We don't support arguments yet*/
void ddxUseMsg()
{
    ErrorF("No extra options yet\n");
}


void ddxInitGlobals()
{
}

int ddxProcessArgument(int argc, char *argv[], int i)
{
    return 0;
}
