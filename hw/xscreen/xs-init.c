#ifdef HAVE_XSCREEN_CONFIG_H
#include <xnest-config.h>
#endif

#include <stdlib.h>

/* need to include Xmd before XCB stuff, or
 * things get redeclared.*/
#include <X11/Xmd.h>
#include <xcb/xcb.h>
#include <xcb/xcb_aux.h>
#include <xcb/xproto.h>
#include <xcb/shape.h>

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


static void xsInitPixmapFormats(const xcb_setup_t *setup, PixmapFormatRec fmts[])
{
    xcb_format_t *bs_fmts; /*formats on backing server*/
    int i;

    bs_fmts = xcb_setup_pixmap_formats(setup);
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
    const xcb_setup_t *setup;
    xcb_screen_t      *screen;
    char           *display;

    /*FIXME: add a "-display" option*/
    /*Globals Globals Everywhere.*/
    xsConnection = xcb_connect(NULL, &screennum);

    if (!xsConnection) { /* failure to connect */
        /* prettify the display name */
        display = getenv("DISPLAY");
        if (!display)
            display = "";
        FatalError("Unable to open display \"%s\".\n", display);
    }

    setup = xcb_get_setup(xsConnection);

    /*set up the root window*/
    screen = xcb_setup_roots_iterator(setup).data;
    xsBackingRoot = screen->root;
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
