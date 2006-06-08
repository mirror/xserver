/* $Xorg: GC.c,v 1.3 2000/08/17 19:53:28 cpqbld Exp $ */
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
/* $XFree86: xc/programs/Xserver/hw/xnest/GC.c,v 3.6 2001/10/28 03:34:11 tsi Exp $ */

#ifdef HAVE_XNEST_CONFIG_H
#include <xnest-config.h>
#endif

#include <X11/Xmd.h>
#include <X11/XCB/xcb.h>
#include <X11/XCB/xproto.h>
#include <X11/XCB/xcb_aux.h>
#include "gcstruct.h"
#include "windowstr.h"
#include "pixmapstr.h"
#include "scrnintstr.h"
#include <X11/fonts/fontstruct.h>
#include "mistruct.h"
#include "region.h"

#include "Xnest.h"

#include "Display.h"
#include "XNGC.h" 
#include "GCOps.h"
#include "Drawable.h"
#include "XNFont.h"
#include "Color.h"

int xnestGCPrivateIndex;

static GCFuncs xnestFuncs = {
    xnestValidateGC,
    xnestChangeGC,
    xnestCopyGC,
    xnestDestroyGC,
    xnestChangeClip,
    xnestDestroyClip,
    xnestCopyClip,
};

static GCOps xnestOps = {
    xnestFillSpans,
    xnestSetSpans,
    xnestPutImage,
    xnestCopyArea, 
    xnestCopyPlane,
    xnestPolyPoint,
    xnestPolylines,
    xnestPolySegment,
    xnestPolyRectangle,
    xnestPolyArc,
    xnestFillPolygon,
    xnestPolyFillRect,
    xnestPolyFillArc,
    xnestPolyText8, 
    xnestPolyText16,
    xnestImageText8,
    xnestImageText16,
    xnestImageGlyphBlt,
    xnestPolyGlyphBlt,
    xnestPushPixels
};

Bool xnestCreateGC(GCPtr pGC)
{
    pGC->clientClipType = CT_NONE;
    pGC->clientClip = NULL;

    pGC->funcs = &xnestFuncs;
    pGC->ops = &xnestOps;

    pGC->miTranslate = 1;

    xnestGCPriv(pGC)->gc = XCBGCONTEXTNew(xnestConnection);
    XCBCreateGC(xnestConnection, 
                xnestGCPriv(pGC)->gc,
                xnestDefaultDrawables[pGC->depth],
                0L,
                NULL);
    xnestGCPriv(pGC)->nClipRects = 0;

    return True;
}

void xnestValidateGC(GCPtr pGC, unsigned long changes, DrawablePtr pDrawable)
{
    pGC->lastWinOrg.x = pDrawable->x;
    pGC->lastWinOrg.y = pDrawable->y;
}

void xnestChangeGC(GCPtr pGC, unsigned long mask)
{
    XCBParamsGC values;

    if (mask & XCBGCFunction)
        values.function = pGC->alu;

    if (mask & XCBGCPlaneMask)
        values.plane_mask = pGC->planemask;

    if (mask & XCBGCForeground)
        values.foreground = xnestPixel(pGC->fgPixel);

    if (mask & XCBGCBackground)
        values.background = xnestPixel(pGC->bgPixel);

    if (mask & XCBGCLineWidth)
        values.line_width = pGC->lineWidth;

    if (mask & XCBGCLineStyle)
        values.line_style = pGC->lineStyle;

    if (mask & XCBGCCapStyle)
        values.cap_style = pGC->capStyle;

    if (mask & XCBGCJoinStyle)
        values.join_style = pGC->joinStyle;

    if (mask & XCBGCFillStyle)
        values.fill_style = pGC->fillStyle;

    if (mask & XCBGCFillRule)
        values.fill_rule = pGC->fillRule;

    if (mask & XCBGCTile) {
        if (pGC->tileIsPixel)
            mask &= ~GCTile;
        else
            values.tile = xnestPixmap(pGC->tile.pixmap).xid;
    }

    if (mask & XCBGCStipple)
        values.stipple = xnestPixmap(pGC->stipple).xid;

    if (mask & XCBGCTileStippleOriginX)
        values.tile_stipple_originX = pGC->patOrg.x;

    if (mask & XCBGCTileStippleOriginY)
        values.tile_stipple_originY = pGC->patOrg.y;

    if (mask & XCBGCFont)
        values.font = xnestFont(pGC->font).xid;

    if (mask & XCBGCSubwindowMode)
        values.subwindow_mode = pGC->subWindowMode;

    if (mask & XCBGCGraphicsExposures)
        values.graphics_exposures = pGC->graphicsExposures;

    if (mask & XCBGCClipOriginY)
        values.clip_originX = pGC->clipOrg.x;

    if (mask & XCBGCClipOriginX)
        values.clip_originY = pGC->clipOrg.y;

    if (mask & XCBGCClipMask) /* this is handled in change clip */
        mask &= ~GCClipMask;

    if (mask & XCBGCDashOffset)
        values.dash_offset = pGC->dashOffset;

    if (mask & XCBGCDashList) {
        mask &= ~GCDashList;
        XCBSetDashes(xnestConnection, 
                     xnestGC(pGC), 
                     pGC->dashOffset, 
                     pGC->numInDashList,
                     (BYTE *)pGC->dash);
    }

    if (mask & XCBGCArcMode)
        values.arc_mode = pGC->arcMode;

    if (mask)
        XCBAuxChangeGC(xnestConnection, xnestGC(pGC), mask, &values);
}

void xnestCopyGC(GCPtr pGCSrc, unsigned long mask, GCPtr pGCDst)
{
    XCBCopyGC(xnestConnection, xnestGC(pGCSrc), xnestGC(pGCDst), mask);
}

void xnestDestroyGC(GCPtr pGC)
{
    XCBFreeGC(xnestConnection, xnestGC(pGC));
}

void xnestChangeClip(GCPtr pGC, int type, pointer pValue, int nRects)
{
    int i, size;
    BoxPtr pBox;
    XCBRECTANGLE *pRects;
    XCBParamsGC param;

    xnestDestroyClipHelper(pGC);

    switch(type) 
    {
        case CT_NONE:
            param.mask = None;
            XCBAuxChangeGC(xnestConnection, xnestGC(pGC), XCBGCClipMask, &param);
            break;

        case CT_REGION:
            nRects = REGION_NUM_RECTS((RegionPtr)pValue);
            size = nRects * sizeof(*pRects);
            pRects = xalloc(size);
            pBox = REGION_RECTS((RegionPtr)pValue);
            for (i = nRects; i-- > 0; ) {
                pRects[i].x = pBox[i].x1;
                pRects[i].y = pBox[i].y1;
                pRects[i].width = pBox[i].x2 - pBox[i].x1;
                pRects[i].height = pBox[i].y2 - pBox[i].y1;
            }
            XCBSetClipRectangles(xnestConnection, 
                    XCBClipOrderingUnsorted,
                    xnestGC(pGC), 
                    0, 0,
                    nRects,
                    pRects);
            xfree((char *) pRects);
            break;

        case CT_PIXMAP:
            param.mask = xnestPixmap((PixmapPtr)pValue).xid;
            XCBAuxChangeGC(xnestConnection, xnestGC(pGC), XCBGCClipMask, &param);
            /*
             * Need to change into region, so subsequent uses are with
             * current pixmap contents.
             */
            pGC->clientClip = (pointer) (*pGC->pScreen->BitmapToRegion)((PixmapPtr)pValue);
            (*pGC->pScreen->DestroyPixmap)((PixmapPtr)pValue);
            pValue = pGC->clientClip;
            type = CT_REGION;
            break;

        case CT_UNSORTED:
            XCBSetClipRectangles(xnestConnection,
                    XCBClipOrderingUnsorted,
                    xnestGC(pGC), 
                    pGC->clipOrg.x,
                    pGC->clipOrg.y,
                    nRects,
                    (XCBRECTANGLE *)pValue);
            break;
        case CT_YSORTED:
            XCBSetClipRectangles(xnestConnection,
                    XCBClipOrderingYSorted,
                    xnestGC(pGC), 
                    pGC->clipOrg.x,
                    pGC->clipOrg.y,
                    nRects,
                    pValue);
            break;
        case CT_YXSORTED:
            XCBSetClipRectangles(xnestConnection,
                    XCBClipOrderingYXSorted,
                    xnestGC(pGC), 
                    pGC->clipOrg.x,
                    pGC->clipOrg.y,
                    nRects,
                    pValue);
            break;
        case CT_YXBANDED:
            XCBSetClipRectangles(xnestConnection,
                    XCBClipOrderingYXBanded,
                    xnestGC(pGC), 
                    pGC->clipOrg.x,
                    pGC->clipOrg.y,
                    nRects,
                    pValue);
            break;
    }

    switch(type) 
    {
        default:
            break;

        case CT_UNSORTED:
        case CT_YSORTED:
        case CT_YXSORTED:
        case CT_YXBANDED:

            /*
             * other parts of server can only deal with CT_NONE,
             * CT_PIXMAP and CT_REGION client clips.
             */
            pGC->clientClip = (pointer) RECTS_TO_REGION(pGC->pScreen, nRects,
                    (xRectangle *)pValue, type);
            xfree(pValue);
            pValue = pGC->clientClip;
            type = CT_REGION;

            break;
    }

    pGC->clientClipType = type;
    pGC->clientClip = pValue;
    xnestGCPriv(pGC)->nClipRects = nRects;
}

void xnestDestroyClip(GCPtr pGC)
{
    XCBParamsGC param;
    param.mask = None;
    xnestDestroyClipHelper(pGC);

    XCBAuxChangeGC(xnestConnection, xnestGC(pGC), XCBGCClipMask, &param);


    pGC->clientClipType = CT_NONE;
    pGC->clientClip = NULL;
    xnestGCPriv(pGC)->nClipRects = 0;
}

void xnestDestroyClipHelper(GCPtr pGC)
{
    switch (pGC->clientClipType)
    {
        default:
        case CT_NONE:
            break;

        case CT_REGION:
            REGION_DESTROY(pGC->pScreen, pGC->clientClip); 
            break;
    }
}

void xnestCopyClip(GCPtr pGCDst, GCPtr pGCSrc)
{
    RegionPtr pRgn;

    switch (pGCSrc->clientClipType)
    {
        default:
        case CT_NONE:
            xnestDestroyClip(pGCDst);
            break;

        case CT_REGION:
            pRgn = REGION_CREATE(pGCDst->pScreen, NULL, 1);
            REGION_COPY(pGCDst->pScreen, pRgn, pGCSrc->clientClip);
            xnestChangeClip(pGCDst, CT_REGION, pRgn, 0);
            break;
    }
}
