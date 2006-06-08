/* $Xorg: Cursor.c,v 1.3 2000/08/17 19:53:28 cpqbld Exp $ */
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
/* $XFree86: xc/programs/Xserver/hw/xnest/Cursor.c,v 1.3 2002/11/23 19:27:50 tsi Exp $ */

#ifdef HAVE_XNEST_CONFIG_H
#include <xnest-config.h>
#endif

#include <X11/Xmd.h>
#include <X11/XCB/xcb.h>
#include <X11/XCB/xcb_aux.h>
#include <X11/XCB/xproto.h>
#include <X11/XCB/xcb_image.h>
#include "screenint.h"
#include "input.h"
#include "misc.h"
#include "cursor.h"
#include "cursorstr.h"
#include "scrnintstr.h"
#include "servermd.h"

#include "Xnest.h"

#include "Display.h"
#include "Screen.h"
#include "XNCursor.h"
#include "Visual.h"
#include "Keyboard.h"
#include "Args.h"

Bool xnestRealizeCursor(ScreenPtr pScreen, CursorPtr pCursor)
{
    XCBImage *ximage;
    XCBPIXMAP source, mask;
    int pad;
    XCBCURSOR c;
    unsigned long valuemask;
    XCBParamsGC values;

    valuemask = XCBGCFunction   | 
                XCBGCPlaneMask  | 
                XCBGCForeground |
                XCBGCBackground |
                XCBGCClipMask;

    values.function = GXcopy;
    values.plane_mask = AllPlanes;
    values.foreground = 1L;
    values.background = 0L;
    values.mask = None;

    XCBAuxChangeGC(xnestConnection, xnestBitmapGC, valuemask, &values);

    source = XCBPIXMAPNew(xnestConnection);
    mask = XCBPIXMAPNew(xnestConnection);

    XCBCreatePixmap(xnestConnection,
                    1,
                    source,
                    (XCBDRAWABLE)xnestDefaultWindows[pScreen->myNum],
                    pCursor->bits->width,
                    pCursor->bits->height);

    XCBCreatePixmap(xnestConnection, 
                    1,
                    mask,
                    (XCBDRAWABLE) xnestDefaultWindows[pScreen->myNum],
                    pCursor->bits->width,
                    pCursor->bits->height);

    pad =  XCBGetSetup(xnestConnection)->bitmap_format_scanline_pad;
    ximage = XCBImageCreate(xnestConnection, 
                            1,
                            XCBImageFormatXYBitmap,
                            0, 
                            (BYTE *)pCursor->bits->source,
                            pCursor->bits->width,
                            pCursor->bits->height,
                            pad,//8,//BitmapPad(xnestConnection),
                            0);
    XCBImageInit(ximage);

    XCBImagePut(xnestConnection,
                (XCBDRAWABLE) source,
                xnestBitmapGC,
                ximage,
                0,
                0,
                0,
                0,
                pCursor->bits->width,
                pCursor->bits->height);

    XFree(ximage);

    ximage = XCBImageCreate(xnestConnection,
                            1,
                            XCBImageFormatXYBitmap,
                            0,
                            (BYTE *)pCursor->bits->mask,
                            pCursor->bits->width,
                            pCursor->bits->height,
                            pad,//8,//BitmapPad(xnestConnection),
                            0);
    XCBImageInit(ximage);

    XCBImagePut(xnestConnection,
                (XCBDRAWABLE)mask,
                xnestBitmapGC,
                ximage,
                0, 0, 0, 0,
                pCursor->bits->width,
                pCursor->bits->height);

    XFree(ximage);

    pCursor->devPriv[pScreen->myNum] = (pointer)xalloc(sizeof(xnestPrivCursor));
    c = XCBCURSORNew(xnestConnection);
    xnestCursorPriv(pCursor, pScreen)->cursor = c.xid;
    XCBCreateCursor(xnestConnection,
                    c,
                    source,
                    mask,
                    pCursor->foreRed,
                    pCursor->foreGreen,
                    pCursor->foreBlue,
                    pCursor->backRed,
                    pCursor->backGreen,
                    pCursor->backBlue,
                    pCursor->bits->xhot,
                    pCursor->bits->yhot);

    XCBFreePixmap(xnestConnection, source);
    XCBFreePixmap(xnestConnection, mask);

    return True;
}

Bool xnestUnrealizeCursor(ScreenPtr pScreen, CursorPtr pCursor)
{
    XCBCURSOR c;
    c.xid = xnestCursor(pCursor, pScreen);
    XCBFreeCursor(xnestConnection, c);
    xfree(xnestCursorPriv(pCursor, pScreen));
    return True;
}

void xnestRecolorCursor(ScreenPtr pScreen, CursorPtr pCursor, Bool displayed)
{
    XCBCURSOR c;
    c.xid = xnestCursor(pCursor, pScreen);
    XCBRecolorCursor(xnestConnection, 
                     c,
                     pCursor->foreRed,
                     pCursor->foreGreen,
                     pCursor->foreBlue,

                     pCursor->backRed,
                     pCursor->backGreen,
                     pCursor->backBlue);
}

void xnestSetCursor (ScreenPtr pScreen, CursorPtr pCursor, int x, int y)
{
    XCBParamsCW params;
    if (pCursor)
    {
        params.cursor = xnestCursor(pCursor, pScreen);
        XCBAuxChangeWindowAttributes(xnestConnection,
                                     xnestDefaultWindows[pScreen->myNum],
                                     XCBCWCursor,
                                     &params);
    }
}

void xnestMoveCursor (ScreenPtr pScreen, int x, int y)
{
}
