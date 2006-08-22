/* $Xorg: Pixmap.c,v 1.3 2000/08/17 19:53:28 cpqbld Exp $ */
/*

Copyright 1993 by Davor Matic

Permission to use, copy, modify, distribute, and sell this software
and its documentation for any purpose is hereby granted without fee,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation.  Davor Matic makes no representations about
the suitability of this software for any purpose.  It is provided "as
is" without express or implied warranty.

*/
/* $XFree86: xc/programs/Xserver/hw/xs/Pixmap.c,v 3.7 2003/07/16 01:38:51 dawes Exp $ */

#ifdef HAVE_XNEST_CONFIG_H
#include <xs-config.h>
#endif

#include <X11/Xmd.h>
#include <X11/XCB/xcb.h>
#include <X11/XCB/xproto.h>
#include <X11/XCB/xcb_image.h> 
#include "regionstr.h"
#include "pixmapstr.h"
#include "scrnintstr.h"
#include "regionstr.h"
#include "gc.h"
#include "servermd.h"
#include "mi.h"

#include "xs-globals.h"
#include "xs-pixmap.h"

#ifdef PIXPRIV
int XS_PIXMAP_PRIVIndex;	    
#endif

static int xsNumFormats;
static XCBFORMAT *xsFormats;

/**
 * Initializes the list of available formats, used
 * in calculating the size and other such places.
 **/
void xsInitFormats()
{
    const XCBSetup *setup;
        
    setup = XCBGetSetup(xsConnection);
    xsNumFormats = XCBSetupPixmapFormatsLength(setup);
    xsFormats = XCBSetupPixmapFormats(setup);
}

/**
 * Calculates the size of a pixmap of a given depth,
 * with a given width and height.
 **/
int xsPixmapCalcSize(int depth, int w, int h)
{
    int size;
    int pad;
    int bpp;
    int i;

    for (i=0; i<xsNumFormats; i++) {
        if (xsFormats[i].depth == depth) {
            pad = xsFormats[i].scanline_pad;
            bpp = xsFormats[i].bits_per_pixel;
            break;
        }
    }
    size = (((bpp * w + pad - 1) & -pad) >> 3)*h;
    return size;
}

/**
 * Creates a new pixmap.
 **/
PixmapPtr xsCreatePixmap(ScreenPtr pScreen, int width, int height, int depth)
{
    PixmapPtr pPixmap;

    pPixmap = AllocatePixmap(pScreen, sizeof(XscreenPrivPixmap));
    if (!pPixmap)
        return NullPixmap;
    pPixmap->drawable.type = DRAWABLE_PIXMAP;
    pPixmap->drawable.class = 0;
    pPixmap->drawable.depth = depth;
    pPixmap->drawable.bitsPerPixel = depth;
    pPixmap->drawable.id = 0;
    pPixmap->drawable.x = 0;
    pPixmap->drawable.y = 0;
    pPixmap->drawable.width = width;
    pPixmap->drawable.height = height;
    pPixmap->drawable.pScreen = pScreen;
    pPixmap->drawable.serialNumber = NEXT_SERIAL_NUMBER;
    pPixmap->refcnt = 1;
    pPixmap->devKind = PixmapBytePad(width, depth);
#ifdef PIXPRIV
    pPixmap->devPrivates[XS_PIXMAP_PRIVateIndex].ptr =
        (pointer)((char *)pPixmap + pScreen->totalPixmapSize);
#else
    pPixmap->devPrivate.ptr = (pointer)(pPixmap + 1);
#endif
    if (width && height){
        XS_PIXMAP_PRIV(pPixmap)->pixmap = XCBPIXMAPNew(xsConnection); 
        XCBCreatePixmap(xsConnection,
                depth,
                XS_PIXMAP_PRIV(pPixmap)->pixmap,
                (XCBDRAWABLE)xsDefaultWindow,
                width, height);
    } else
        XS_PIXMAP_PRIV(pPixmap)->pixmap.xid = 0;

    return pPixmap;
}

/**
 * Destroys a pixmap*
 **/
Bool xsDestroyPixmap(PixmapPtr pPixmap)
{
    if(--pPixmap->refcnt)
        return TRUE;
    XCBFreePixmap(xsConnection, XS_PIXMAP_PRIV(pPixmap)->pixmap);
    xfree(pPixmap);
    return TRUE;
}

/**
 * Converts a pixmap to a region
 **/
RegionPtr xsPixmapToRegion(PixmapPtr pPixmap)
{
    XCBImage *ximage;
    register RegionPtr pReg, pTmpReg;
    register int x, y;
    unsigned long previousPixel, currentPixel;
    BoxRec Box;
    Bool overlap;

    ximage = XCBImageGet(xsConnection,
                        (XCBDRAWABLE)XS_PIXMAP_PRIV(pPixmap)->pixmap,
                        0, 0,
                        pPixmap->drawable.width, pPixmap->drawable.height,
                        1,
                        XYPixmap);

    pReg = REGION_CREATE(pPixmap->drawable.pScreen, NULL, 1);
    pTmpReg = REGION_CREATE(pPixmap->drawable.pScreen, NULL, 1);
    if(!pReg || !pTmpReg) {
        XCBImageDestroy(ximage);
        return NullRegion;
    }

    for (y = 0; y < pPixmap->drawable.height; y++) {
        Box.y1 = y;
        Box.y2 = y + 1;
        previousPixel = 0L;
        for (x = 0; x < pPixmap->drawable.width; x++) {
            currentPixel = XCBImageGetPixel(ximage, x, y);
            if (previousPixel != currentPixel) {
                if (previousPixel == 0L) { 
                    /* left edge */
                    Box.x1 = x;
                }
                else if (currentPixel == 0L) {
                    /* right edge */
                    Box.x2 = x;
                    REGION_RESET(pPixmap->drawable.pScreen, pTmpReg, &Box);
                    REGION_APPEND(pPixmap->drawable.pScreen, pReg, pTmpReg);
                }
                previousPixel = currentPixel;
            }
        }
        if (previousPixel != 0L) {
            /* right edge because of the end of pixmap */
            Box.x2 = pPixmap->drawable.width;
            REGION_RESET(pPixmap->drawable.pScreen, pTmpReg, &Box);
            REGION_APPEND(pPixmap->drawable.pScreen, pReg, pTmpReg);
        }
    }

    REGION_DESTROY(pPixmap->drawable.pScreen, pTmpReg);
    XCBImageDestroy(ximage);

    REGION_VALIDATE(pPixmap->drawable.pScreen, pReg, &overlap);

    return(pReg);
}
