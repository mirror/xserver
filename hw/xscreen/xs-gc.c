/* $Xorg: GC.c,v 1.3 2000/08/17 19:53:28 cpqbld Exp $ */
/*

   Copyright 2006 Ori Bernstein
   Permission to use, copy, modify, distribute, and sell this software
   and its documentation for any purpose is hereby granted without fee,
   provided that the above copyright notice appear in all copies and that
   both that copyright notice and this permission notice appear in
   supporting documentation. Ori Bernstein makes no representations about
   the suitability of this software for any purpose.  It is provided "as
   is" without express or implied warranty.

*/
/* $XFree86: xc/programs/Xserver/hw/xs/GC.c,v 3.6 2001/10/28 03:34:11 tsi Exp $ */

#ifdef HAVE_XNEST_CONFIG_H
#include <xs-config.h>
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

#include "xs-globals.h"
#include "xs-pixmap.h"
#include "xs-font.h"
#include "xs-gcops.h"
#include "xs-gc.h"

int XS_GC_PRIVateIndex;

static GCFuncs xsFuncs = {
    xsValidateGC,
    xsChangeGC,
    xsCopyGC,
    xsDestroyGC,
    xsChangeClip,
    xsDestroyClip,
    xsCopyClip,
};

static GCOps xsOps = {
    xsFillSpans,
    xsSetSpans,
    xsPutImage,
    xsCopyArea, 
    xsCopyPlane,
    xsPolyPoint,
    xsPolylines,
    xsPolySegment,
    xsPolyRectangle,
    xsPolyArc,
    xsFillPolygon,
    xsPolyFillRect,
    xsPolyFillArc,
    xsPolyText8, 
    xsPolyText16,
    xsImageText8,
    xsImageText16,
    xsImageGlyphBlt,
    xsPolyGlyphBlt,
    xsPushPixels
};

Bool xsCreateGC(GCPtr pGC)
{
    pGC->clientClipType = CT_NONE;
    pGC->clientClip = NULL;

    pGC->funcs = &xsFuncs;
    pGC->ops = &xsOps;

    pGC->miTranslate = 1;

    XS_GC_PRIV(pGC)->gc = XCBGCONTEXTNew(xsConnection);
    XCBCreateGC(xsConnection, 
                XS_GC_PRIV(pGC)->gc,
                xsDefaultDrawables[pGC->depth],
                0L,
                NULL);
    //XS_GC_PRIV(pGC)->nClipRects = 0;

    return TRUE;
}

void xsValidateGC(GCPtr pGC, unsigned long changes, DrawablePtr pDrawable)
{
    pGC->lastWinOrg.x = pDrawable->x;
    pGC->lastWinOrg.y = pDrawable->y;
}

void xsChangeGC(GCPtr pGC, unsigned long mask)
{
    XCBParamsGC values;

    if (mask & XCBGCFunction)
        values.function = pGC->alu;

    if (mask & XCBGCPlaneMask)
        values.plane_mask = pGC->planemask;

    if (mask & XCBGCForeground)
        values.foreground = pGC->fgPixel;

    if (mask & XCBGCBackground)
        values.background = pGC->bgPixel;

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
            values.tile = XS_PIXMAP_PRIV(pGC->tile.pixmap)->pixmap.xid;
    }

    if (mask & XCBGCStipple)
        values.stipple = XS_PIXMAP_PRIV(pGC->stipple)->pixmap.xid;

    if (mask & XCBGCTileStippleOriginX)
        values.tile_stipple_originX = pGC->patOrg.x;

    if (mask & XCBGCTileStippleOriginY)
        values.tile_stipple_originY = pGC->patOrg.y;

    if (mask & XCBGCFont)
        values.font = XS_FONT_PRIV(pGC->font)->font.xid;

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
        XCBSetDashes(xsConnection, 
                     XS_GC_PRIV(pGC)->gc, 
                     pGC->dashOffset, 
                     pGC->numInDashList,
                     (BYTE *)pGC->dash);
    }

    if (mask & XCBGCArcMode)
        values.arc_mode = pGC->arcMode;

    if (mask)
        XCBAuxChangeGC(xsConnection, XS_GC_PRIV(pGC)->gc, mask, &values);
}

void xsCopyGC(GCPtr pGCSrc, unsigned long mask, GCPtr pGCDst)
{
    XCBCopyGC(xsConnection, XS_GC_PRIV(pGCSrc)->gc, XS_GC_PRIV(pGCDst)->gc, mask);
}

void xsDestroyGC(GCPtr pGC)
{
    XCBFreeGC(xsConnection, XS_GC_PRIV(pGC)->gc);
}

void xsChangeClip(GCPtr pGC, int type, pointer pValue, int nRects)
{
    int i, size;
    BoxPtr pBox;
    XCBRECTANGLE *pRects;
    XCBParamsGC param;

    xsDestroyClipHelper(pGC);

    switch(type) 
    {
        case CT_NONE:
            param.mask = None;
            XCBAuxChangeGC(xsConnection, XS_GC_PRIV(pGC)->gc, XCBGCClipMask, &param);
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
            XCBSetClipRectangles(xsConnection, 
                    XCBClipOrderingUnsorted,
                    XS_GC_PRIV(pGC)->gc, 
                    0, 0,
                    nRects,
                    pRects);
            xfree((char *) pRects);
            break;

        case CT_PIXMAP:
            param.mask = XS_PIXMAP_PRIV((PixmapPtr)pValue)->pixmap.xid;
            XCBAuxChangeGC(xsConnection, XS_GC_PRIV(pGC)->gc, XCBGCClipMask, &param);
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
            XCBSetClipRectangles(xsConnection,
                    XCBClipOrderingUnsorted,
                    XS_GC_PRIV(pGC)->gc, 
                    pGC->clipOrg.x,
                    pGC->clipOrg.y,
                    nRects,
                    (XCBRECTANGLE *)pValue);
            break;
        case CT_YSORTED:
            XCBSetClipRectangles(xsConnection,
                    XCBClipOrderingYSorted,
                    XS_GC_PRIV(pGC)->gc, 
                    pGC->clipOrg.x,
                    pGC->clipOrg.y,
                    nRects,
                    pValue);
            break;
        case CT_YXSORTED:
            XCBSetClipRectangles(xsConnection,
                    XCBClipOrderingYXSorted,
                    XS_GC_PRIV(pGC)->gc, 
                    pGC->clipOrg.x,
                    pGC->clipOrg.y,
                    nRects,
                    pValue);
            break;
        case CT_YXBANDED:
            XCBSetClipRectangles(xsConnection,
                    XCBClipOrderingYXBanded,
                    XS_GC_PRIV(pGC)->gc, 
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
    //XS_GC_PRIV(pGC)->nClipRects = nRects;
}

void xsDestroyClip(GCPtr pGC)
{
    XCBParamsGC param;
    param.mask = None;
    xsDestroyClipHelper(pGC);

    XCBAuxChangeGC(xsConnection, XS_GC_PRIV(pGC)->gc, XCBGCClipMask, &param);


    pGC->clientClipType = CT_NONE;
    pGC->clientClip = NULL;
    //XS_GC_PRIV(pGC)->nClipRects = 0;
}

void xsDestroyClipHelper(GCPtr pGC)
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

void xsCopyClip(GCPtr pGCDst, GCPtr pGCSrc)
{
    RegionPtr pRgn;

    switch (pGCSrc->clientClipType)
    {
        default:
        case CT_NONE:
            xsDestroyClip(pGCDst);
            break;

        case CT_REGION:
            pRgn = REGION_CREATE(pGCDst->pScreen, NULL, 1);
            REGION_COPY(pGCDst->pScreen, pRgn, pGCSrc->clientClip);
            xsChangeClip(pGCDst, CT_REGION, pRgn, 0);
            break;
    }
}
